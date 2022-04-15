/*
 * Copyright (c) 1998-2001
 * University of Southern California/Information Sciences Institute.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 *  $Id: igmp_proto.c,v 1.15 2001/09/10 20:31:36 pavlin Exp $
 */
/*
 * Part of this program has been derived from mrouted.
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE.mrouted".
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 */

#include "defs.h"

typedef struct {
    vifi_t  vifi;
    struct listaddr *g;
    uint32_t source; /* Source for SSM */
    int q_time; /* IGMP Code */
    int q_len; /* Data length */
} cbk_t;


/*
 * Forward declarations.
 */
static void DelVif       (void *arg);
static int SetTimer      (vifi_t vifi, struct listaddr *g, uint32_t source);
static int SetVerTimer   (vifi_t vifi, struct listaddr *g);
static int DeleteTimer   (int id);
static void send_query   (struct uvif *v, uint32_t group, int interval);
static void SendQuery    (void *arg);
static int SetQueryTimer (struct listaddr *g, vifi_t vifi, int to_expire, int q_time, int q_len);
static uint32_t igmp_group_membership_timeout(void);

/* The querier timeout depends on the configured query interval */
uint32_t igmp_query_interval  = IGMP_QUERY_INTERVAL;
uint32_t igmp_querier_timeout = IGMP_OTHER_QUERIER_PRESENT_INTERVAL;


/*
 * Send group membership queries on that interface if I am querier.
 */
void query_groups(struct uvif *v)
{
    struct listaddr *g;

    if (v->uv_stquery_cnt)
	v->uv_stquery_cnt--;
    if (v->uv_stquery_cnt)
	v->uv_gq_timer = igmp_query_interval / 4;
    else
	v->uv_gq_timer = igmp_query_interval;

    if (v->uv_flags & VIFF_QUERIER) {
	int datalen;
	int code;

	/* IGMPv2 and v3 */
	code = IGMP_MAX_HOST_REPORT_DELAY * IGMP_TIMER_SCALE;

	/*
	 * IGMP version to use depends on the compatibility mode of the
	 * interface, which may channge at runtime depending on version
	 * of the end devices on a segment.
	 */
	if (v->uv_flags & VIFF_IGMPV1) {
	    /*
	     * RFC 3376: When in IGMPv1 mode, routers MUST send Periodic
	     *           Queries with a Max Response Time of 0
	     */
	    datalen = 0;
	    code = 0;
	} else if (v->uv_flags & VIFF_IGMPV2) {
	    /*
	     * RFC 3376: When in IGMPv2 mode, routers MUST send Periodic
	     *           Queries truncated at the Group Address field
	     *           (i.e., 8 bytes long)
	     */
	    datalen = 0;
	} else {
	    /*
	     * RFC 3376: Length determines v3 or v2 query
	     */
	    datalen = 4;
	}

	IF_DEBUG(DEBUG_IGMP)
	    logit(LOG_DEBUG, 0, "Sending IGMP v%s query on %s",
		  datalen == 4 ? "3" : "2", v->uv_name);

	send_igmp(igmp_send_buf, v->uv_lcl_addr, allhosts_group,
		  IGMP_MEMBERSHIP_QUERY,
		  code, 0, datalen);
    }

    /*
     * Decrement the old-hosts-present timer for each
     * active group on that vif.
     */
    for (g = v->uv_groups; g != NULL; g = g->al_next) {
	if (g->al_old > TIMER_INTERVAL)
	    g->al_old -= TIMER_INTERVAL;
	else
	    g->al_old = 0;
    }
}


/*
 * Process an incoming host membership query
 */
void accept_membership_query(int ifi, uint32_t src, uint32_t dst, uint32_t group, int tmo, int igmp_version)
{
    vifi_t vifi;
    struct uvif *v;

    (void)dst;

    /* Ignore my own membership query */
    if (local_address(src) != NO_VIF)
	return;

    /* TODO: modify for DVMRP?? */
    if ((vifi = find_vif(ifi)) == NO_VIF &&
	 (vifi = find_vif_direct(src)) == NO_VIF) {
	IF_DEBUG(DEBUG_IGMP)
	    logit(LOG_INFO, 0, "Ignoring group membership query from non-adjacent host %s",
		  inet_fmt(src, s1, sizeof(s1)));
	return;
    }

    v = &uvifs[vifi];

    /* Do not accept messages of higher version than current
     * compatibility mode as specified in RFC 3376 - 7.3.1
     */
    if (v->uv_querier) {
	if ((igmp_version == 3 && (v->uv_flags & VIFF_IGMPV2)) ||
	    (igmp_version == 2 && (v->uv_flags & VIFF_IGMPV1))) {
	    int i;

	    /*
	     * Exponentially back-off warning rate
	     */
	    i = ++v->uv_igmpv1_warn;
	    while (i && !(i & 1))
		i >>= 1;
	    if (i == 1)
		logit(LOG_WARNING, 0, "Received IGMP v%d query from %s on %s,"
		      " but interface is in IGMP v%d network compatibility mode",
		      igmp_version, inet_fmt(src, s1, sizeof(s1)),
		      v->uv_name, v->uv_flags & VIFF_IGMPV1 ? 1 : 2);
	    return;
	}
    }

    if (!v->uv_querier || v->uv_querier->al_addr != src) {
	uint32_t cur = v->uv_querier ? v->uv_querier->al_addr : v->uv_lcl_addr;

	/*
	 * This might be:
	 * - A query from a new querier, with a lower source address
	 *   than the current querier (who might be me)
	 * - A query from a new router that just started up and doesn't
	 *   know who the querier is.
	 * - A proxy query (source address 0.0.0.0), never wins elections
	 */
	if (!ntohl(src)) {
	    logit(LOG_DEBUG, 0, "Ignoring proxy query on %s", v->uv_name);
	    return;
	}

	if (ntohl(src) < ntohl(cur)) {
	    IF_DEBUG(DEBUG_IGMP) {
		logit(LOG_DEBUG, 0, "new querier %s (was %s) on %s", inet_fmt(src, s1, sizeof(s1)),
		      v->uv_querier ? inet_fmt(v->uv_querier->al_addr, s2, sizeof(s2)) : "me",
		      v->uv_name);
	    }

	    if (!v->uv_querier) {
		v->uv_querier = calloc(1, sizeof(struct listaddr));
		if (!v->uv_querier) {
		    logit(LOG_ERR, 0, "%s(): out of memory", __func__);
		    return;
		}

		v->uv_querier->al_next = (struct listaddr *)NULL;
		v->uv_querier->al_timer = 0;
		v->uv_querier->al_genid = 0;
		v->uv_querier->al_mv = 0;
		v->uv_querier->al_old = 0;
		v->uv_querier->al_index = 0;
		v->uv_querier->al_timerid = 0;
		v->uv_querier->al_query = 0;
		v->uv_querier->al_flags = 0;

		v->uv_flags &= ~VIFF_QUERIER;
	    }
	    v->uv_querier->al_addr = src;
	    time(&v->uv_querier->al_ctime);
	}
    }

    /*
     * Reset the timer since we've received a query.
     */
    if (v->uv_querier && src == v->uv_querier->al_addr)
	v->uv_querier->al_timer = 0;

    /*
     * If this is a Group-Specific query which we did not source,
     * we must set our membership timer to [Last Member Query Count] *
     * the [Max Response Time] in the packet.
     */
    if (!(v->uv_flags & VIFF_IGMPV1) && group != 0 && src != v->uv_lcl_addr) {
	struct listaddr *g;

	IF_DEBUG(DEBUG_IGMP) {
	    logit(LOG_DEBUG, 0, "Group-specific membership query for %s from %s on vif %d, timer %d",
		  inet_fmt(group, s2, sizeof(s2)), inet_fmt(src, s1, sizeof(s1)), vifi, tmo);
	}

	for (g = v->uv_groups; g != NULL; g = g->al_next) {
	    if (group == g->al_addr && g->al_query == 0) {
		/* setup a timeout to remove the group membership */
		if (g->al_timerid)
		    g->al_timerid = DeleteTimer(g->al_timerid);

		g->al_timer = IGMP_LAST_MEMBER_QUERY_COUNT * tmo / IGMP_TIMER_SCALE;
		/* use al_query to record our presence in last-member state */
		g->al_query = -1;
		g->al_timerid = SetTimer(vifi, g, 0);
		IF_DEBUG(DEBUG_IGMP) {
		    logit(LOG_DEBUG, 0, "Timer for grp %s on vif %d set to %u",
			  inet_fmt(group, s2, sizeof(s2)), vifi, g->al_timer);
		}
		break;
	    }
	}
    }
}


/*
 * Process an incoming group membership report.
 */
void accept_group_report(int ifi, uint32_t igmp_src, uint32_t ssm_src, uint32_t group, int igmp_report_type)
{
    vifi_t vifi;
    struct uvif *v;
    struct listaddr *g;
    struct listaddr *s = NULL;

    inet_fmt(igmp_src, s1, sizeof(s1));
    inet_fmt(ssm_src, s2, sizeof(s2));
    inet_fmt(group, s3, sizeof(s3));

    /* Do not filter LAN scoped groups */
    if (ntohl(group) <= INADDR_MAX_LOCAL_GROUP) { /* group <= 224.0.0.255? */
	IF_DEBUG(DEBUG_IGMP)
	    logit(LOG_DEBUG, 0, "    %-16s LAN scoped group, skipping.", s3);
	return;
    }

    if ((vifi = find_vif(ifi)) == NO_VIF &&
	 (vifi = find_vif_direct(igmp_src)) == NO_VIF) {
	IF_DEBUG(DEBUG_IGMP) {
	    logit(LOG_INFO, 0, "Ignoring group membership report from non-adjacent host %s",
		  inet_fmt(igmp_src, s1, sizeof(s1)));
	}
	return;
    }

    IF_DEBUG(DEBUG_IGMP)
	logit(LOG_DEBUG, 0, "%s(): igmp_src %s ssm_src %s group %s report_type %i",
	      __func__, s1, s2, s3, igmp_report_type);

    v = &uvifs[vifi];

    /*
     * Look for the group in our group list; if found, reset its timer.
     */
    for (g = v->uv_groups; g != NULL; g = g->al_next) {
	if (group == g->al_addr) {
	    int old_report = 0;

	    if (igmp_report_type == IGMP_V1_MEMBERSHIP_REPORT) {
		g->al_old = DVMRP_OLD_AGE_THRESHOLD;
		old_report = 1;

		if (g->al_pv > 1) {
		    IF_DEBUG(DEBUG_IGMP)
			logit(LOG_DEBUG, 0, "Change IGMP compatibility mode to v1 for group %s", s3);
		    g->al_pv = 1;
		}
	    } else if (igmp_report_type == IGMP_V2_MEMBERSHIP_REPORT) {
		old_report = 1;

		if (g->al_pv > 2) {
		    IF_DEBUG(DEBUG_IGMP)
			logit(LOG_DEBUG, 0, "Change IGMP compatibility mode to v2 for group %s", s3);
		    g->al_pv = 2;
		}
	    }

	    g->al_reporter = igmp_src;

	    /** delete old timers, set a timer for expiration **/
	    g->al_timer = igmp_group_membership_timeout();
	    if (g->al_query)
		g->al_query = DeleteTimer(g->al_query);

	    if (g->al_timerid)
		g->al_timerid = DeleteTimer(g->al_timerid);

	    g->al_timerid = SetTimer(vifi, g, ssm_src);

	    /* Reset timer for switching version back every time an older version report is received */
	    if (g->al_pv < 3 && old_report) {
		if (g->al_versiontimer)
			g->al_versiontimer = DeleteTimer(g->al_versiontimer);

		g->al_versiontimer = SetVerTimer(vifi, g);
	    }

	    /* Find source */
	    if (IN_PIM_SSM_RANGE(group)) {
		for (s = g->al_sources; s; s = s->al_next) {
		    IF_DEBUG(DEBUG_IGMP)
			logit(LOG_DEBUG, 0, "%s(): Seek source %s, curr=%s", __func__,
			      inet_fmt(ssm_src, s1, sizeof(s1)),
			      inet_fmt(s->al_addr, s2, sizeof(s2)));
		    if (ssm_src == s->al_addr) {
			IF_DEBUG(DEBUG_IGMP)
			    logit(LOG_DEBUG, 0, "%s(): Source found", __func__);
			break;
		    }
		}
		if (!s) {
		    /* Add new source */
		    s = calloc(1, sizeof(struct listaddr));
		    if (!s) {
			logit(LOG_ERR, errno, "%s(): Ran out of memory", __func__);
			return;
		    }
		    s->al_addr = ssm_src;
		    s->al_next = g->al_sources;
		    g->al_sources = s;

		    IF_DEBUG(DEBUG_IGMP)
			logit(LOG_DEBUG, 0, "%s(): Source %s added to g:%p", __func__, s2, g);
		}
	    }

	    /* TODO: might need to add a check if I am the forwarder??? */
	    /* if (v->uv_flags & VIFF_DR) */
	    if (IN_PIM_SSM_RANGE(group)) {
		IF_DEBUG(DEBUG_IGMP)
		    logit(LOG_INFO, 0, "Add leaf (%s,%s)", s1, s3);
		add_leaf(vifi, ssm_src, group);
	    } else {
		IF_DEBUG(DEBUG_IGMP)
		    logit(LOG_INFO, 0, "Add leaf (*,%s)", s3);
		add_leaf(vifi, INADDR_ANY_N, group);
	    }
	    break;
	}
    }

    /*
     * If not found, add it to the list and update kernel cache.
     */
    if (!g) {
	g = calloc(1, sizeof(struct listaddr));
	if (!g) {
	    logit(LOG_ERR, errno, "%s(): Ran out of memory", __func__);
	    return;
	}

	g->al_addr = group;
	if (igmp_report_type == IGMP_V1_MEMBERSHIP_REPORT) {
	    g->al_old = DVMRP_OLD_AGE_THRESHOLD;
	    IF_DEBUG(DEBUG_IGMP)
		logit(LOG_DEBUG, 0, "Change IGMP compatibility mode to v1 for group %s", s3);
	    g->al_pv = 1;
	} else if (igmp_report_type == IGMP_V2_MEMBERSHIP_REPORT) {
	    IF_DEBUG(DEBUG_IGMP)
		logit(LOG_DEBUG, 0, "Change IGMP compatibility mode to v2 for group %s", s3);
	    g->al_pv = 2;
	} else {
	    g->al_pv = 3;
	}

	/* Add new source */
	if (IN_PIM_SSM_RANGE(group)) {
	    s = calloc(1, sizeof(struct listaddr));
	    if (!s) {
		logit(LOG_ERR, errno, "%s(): Ran out of memory", __func__);
		return;
	    }
	    s->al_addr = ssm_src;
	    s->al_next = g->al_sources;
	    g->al_sources = s;
	    IF_DEBUG(DEBUG_IGMP)
		logit(LOG_DEBUG, 0, "%s(): Source %s added to new g:%p", __func__, s2, g);
	}

	/** set a timer for expiration **/
	g->al_query     = 0;
	g->al_timer     = igmp_group_membership_timeout();
	g->al_reporter  = igmp_src;
	g->al_timerid   = SetTimer(vifi, g, ssm_src);

	/* Set timer for swithing version back if an older version report is received */
	if (g->al_pv < 3)
	    g->al_versiontimer = SetVerTimer(vifi, g);

	g->al_next      = v->uv_groups;
	v->uv_groups    = g;
	time(&g->al_ctime);

	/* TODO: might need to add a check if I am the forwarder??? */
	/* if (v->uv_flags & VIFF_DR) */
	if (IN_PIM_SSM_RANGE(group)) {
	    IF_DEBUG(DEBUG_IGMP)
		logit(LOG_INFO, 0, "SSM group order from  %s (%s,%s)", s1, s2, s3);
	    add_leaf(vifi, ssm_src, group);
	} else {
	    IF_DEBUG(DEBUG_IGMP)
		logit(LOG_INFO, 0, "ASM group order from  %s (*,%s)", s1, s3);
	    add_leaf(vifi, INADDR_ANY_N, group);
	}
    }
}


/* TODO: send PIM prune message if the last member? */
void accept_leave_message(int ifi, uint32_t src, uint32_t dst, uint32_t group)
{
    vifi_t vifi;
    struct uvif *v;
    struct listaddr *g;

    inet_fmt(src, s1, sizeof(s1));
    inet_fmt(dst, s2, sizeof(s2));
    inet_fmt(group, s3, sizeof(s3));

    /* TODO: modify for DVMRP ??? */
    if ((vifi = find_vif(ifi)) == NO_VIF &&
	 (vifi = find_vif_direct(src)) == NO_VIF) {
	IF_DEBUG(DEBUG_IGMP)
	    logit(LOG_INFO, 0, "ignoring group leave report from non-adjacent host %s",
		  inet_fmt(src, s1, sizeof(s1)));
	return;
    }

    IF_DEBUG(DEBUG_IGMP)
	logit(LOG_DEBUG, 0, "%s(): src %s dst %s group %s", __func__, s1, s2, s3);

    v = &uvifs[vifi];

#if 0
    /* XXX: a PIM-SM last-hop router needs to know when a local member
     * has left.
     */
    if (!(v->uv_flags & (VIFF_QUERIER | VIFF_DR))
	|| (v->uv_flags & VIFF_IGMPV1))
	return;
#endif

    /*
     * Look for the group in our group list in order to set up a short-timeout
     * query.
     */
    for (g = v->uv_groups; g; g = g->al_next) {
	int datalen;
	int code;

	if (group == g->al_addr) {
	    IF_DEBUG(DEBUG_IGMP)
		logit(LOG_DEBUG, 0, "%s(): old=%d query=%d", __func__, g->al_old, g->al_query);

	    /* Ignore the leave message if there are old hosts present */
	    if (g->al_old)
		return;

	    /* still waiting for a reply to a query, ignore the leave */
	    if (g->al_query)
		return;

	    /*
	     * Remove source.  Ignore leave if there are more sources
	     * left.  When processing TO_IN({}), remove all sources.
	     */
	    if (IN_PIM_SSM_RANGE(g->al_addr)) {
		struct listaddr *curr, *prev = NULL;
		int removed = 0;

		for (curr = g->al_sources; curr; prev = curr, curr = curr->al_next) {
		    if (dst && dst != curr->al_addr)
			continue;

		    if (!prev)
			g->al_sources = curr->al_next; /* Remove from beginning */
		    else
			prev->al_next = curr->al_next;

		    /* Stop any switch_version() timer */
		    timer_clear(curr->al_versiontimer);
		    free(curr);
		    removed = 1;
		    break;
		}

		/* still sources left, don't remove group */
		if (removed && g->al_sources)
		    return;
	    }

	    /** delete old timer set a timer for expiration **/
	    if (g->al_timerid)
		g->al_timerid = DeleteTimer(g->al_timerid);

#if IGMP_LAST_MEMBER_QUERY_COUNT != 2
/*
  This code needs to be updated to keep a counter of the number
  of queries remaining.
*/
#endif

	    /* IGMPv2 and v3 */
	    code = IGMP_LAST_MEMBER_QUERY_INTERVAL * IGMP_TIMER_SCALE;

	    /* Use lowest IGMP version */
	    if (v->uv_flags & VIFF_IGMPV2 || g->al_pv <= 2) {
		datalen = 0;
	    } else if (v->uv_flags & VIFF_IGMPV1 || g->al_pv == 1) {
		datalen = 0;
		code = 0;
	    } else {
		datalen = 4;
	    }

	    /** send a group specific querry **/
	    if (v->uv_flags & VIFF_QUERIER) {
		IF_DEBUG(DEBUG_IGMP)
		    logit(LOG_DEBUG, 0, "%s(): Sending IGMP v%s query (al_pv=%d)",
			  __func__, datalen == 4 ? "3" : "2", g->al_pv);

		send_igmp(igmp_send_buf, v->uv_lcl_addr, g->al_addr,
			  IGMP_MEMBERSHIP_QUERY, code, g->al_addr, datalen);
	    }

	    g->al_timer = IGMP_LAST_MEMBER_QUERY_INTERVAL * (IGMP_LAST_MEMBER_QUERY_COUNT + 1);
	    g->al_query = SetQueryTimer(g, vifi, IGMP_LAST_MEMBER_QUERY_INTERVAL, code, datalen);
	    g->al_timerid = SetTimer(vifi, g, dst);
	    break;
	}
    }
}

/*
 * Loop through and process all sources in a v3 record.
 */
int accept_sources(int ifi, int type, uint32_t src, uint32_t group, uint8_t *sources, uint8_t *canary, int num_sources)
{
    uint8_t *s;
    int j;

    for (j = 0, s = sources; j < num_sources; ++j, s += 4) {
	in_addr_t ina = ((struct in_addr *)s)->s_addr;

        if ((s + 4) > canary) {
	    IF_DEBUG(DEBUG_IGMP)
		logit(LOG_DEBUG, 0, "Invalid IGMPv3 report, too many sources, would overflow.");
            return 1;
        }

        accept_group_report(ifi, src, ina, group, type);

	IF_DEBUG(DEBUG_IGMP)
	    logit(LOG_DEBUG, 0, "Accepted, switch SPT (%s,%s)", inet_fmt(ina, s1, sizeof(s1)),
		  inet_fmt(group, s2, sizeof(s2)));

        switch_shortest_path(ina, group);
    }

    return 0;
}

/*
 * Handle IGMP v3 membership reports (join/leave)
 */
void accept_membership_report(int ifi, uint32_t src, uint32_t dst, struct igmpv3_report *report, ssize_t reportlen)
{
    uint8_t *canary = (uint8_t *)report + reportlen;
    struct igmpv3_grec *record;
    int num_groups, i;

    num_groups = ntohs(report->ngrec);
    if (num_groups < 0) {
	logit(LOG_INFO, 0, "Invalid Membership Report from %s: num_groups = %d",
	      inet_fmt(src, s1, sizeof(s1)), num_groups);
	return;
    }

    IF_DEBUG(DEBUG_IGMP)
	logit(LOG_DEBUG, 0, "IGMP v3 report, %zd bytes, from %s to %s with %d group records.",
	      reportlen, inet_fmt(src, s1, sizeof(s1)), inet_fmt(dst, s2, sizeof(s2)), num_groups);

    record = &report->grec[0];

    for (i = 0; i < num_groups; i++) {
	struct in_addr  rec_group;
	uint8_t        *sources;
	int             rec_type;
	int             rec_auxdatalen;
	int             rec_num_sources;
	int             j, rc;
	char src_str[200];
	int record_size = 0;

	rec_num_sources = ntohs(record->grec_nsrcs);
	rec_auxdatalen = record->grec_auxwords;
	record_size = sizeof(struct igmpv3_grec) + sizeof(uint32_t) * rec_num_sources + rec_auxdatalen;
	if ((uint8_t *)record + record_size > canary) {
	    logit(LOG_INFO, 0, "Invalid group report %p > %p",
		  (uint8_t *)record + record_size, canary);
	    return;
	}

	rec_type = record->grec_type;
	rec_group.s_addr = (in_addr_t)record->grec_mca;
	sources = (uint8_t *)record->grec_src;
	switch (rec_type) {
	    case IGMP_MODE_IS_EXCLUDE:
	    case IGMP_CHANGE_TO_EXCLUDE_MODE:
		/* RFC 4604: A router SHOULD ignore a group record of
		 *           type TO_EX/IS_EX if it refers to an SSM
		 *           group.  Use ALLOW/BLOCK instead.
		 */
		if (IN_PIM_SSM_RANGE(rec_group.s_addr)) {
		    logit(LOG_DEBUG, 0, "TO_EX/IS_EX %s, reserved for SSM", inet_ntoa(rec_group));
		    break;
		}

		if (rec_num_sources == 0) {
		    /* RFC 5790: TO_EX({}) can be interpreted as a (*,G)
		     *           join, i.e., to include all sources.
		     */
		    accept_group_report(ifi, src, 0, rec_group.s_addr, report->type);
		} else {
		    /* RFC 5790: LW-IGMPv3 does not use TO_EX({x}),
		     *           i.e., filter with non-null source.
		     */
		    logit(LOG_DEBUG, 0, "IS_EX/TO_EX({x}), not unsupported, RFC5790.");
		}
		break;

	    case IGMP_MODE_IS_INCLUDE:
	    case IGMP_CHANGE_TO_INCLUDE_MODE:
		if (rec_num_sources == 0) {
		    /* RFC5790: TO_IN({}) can be interpreted as an
		     *          IGMPv2 (*,G) leave.
		     */
		    accept_leave_message(ifi, src, 0, rec_group.s_addr);
		    break;
		} else {
		    /* RFC5790: TO_IN({x}), regular RFC3376 (S,G)
		     *          join with >= 1 source, 'S'.
		     */
		    rc = accept_sources(ifi, report->type, src, rec_group.s_addr,
					sources, canary, rec_num_sources);
		    if (rc)
			return;
		}
		break;

	    case IGMP_ALLOW_NEW_SOURCES:
		/* RFC5790: Same as TO_IN({x}) */
		rc = accept_sources(ifi, report->type, src, rec_group.s_addr,
				    sources, canary, rec_num_sources);
		if (rc)
		    return;
		break;

	    case IGMP_BLOCK_OLD_SOURCES:
		/* RFC5790: Instead of TO_EX({x}) */
		for (j = 0; j < rec_num_sources; j++) {
		    uint8_t *gsrc = (uint8_t *)&record->grec_src[j];

		    if (gsrc > canary) {
			logit(LOG_INFO, 0, "Invalid group record");
			return;
		    }

		    IF_DEBUG(DEBUG_IGMP)
			logit(LOG_DEBUG, 0, "Remove source[%d] (%s,%s)", j,
			      inet_fmt(record->grec_src[j], s1, sizeof(s1)),
			      inet_ntoa(rec_group));
		    accept_leave_message(ifi, src, record->grec_src[j], rec_group.s_addr);
		    IF_DEBUG(DEBUG_IGMP)
			logit(LOG_DEBUG, 0, "Accepted");
		}
		break;

	    default:
		/* RFC3376: Unrecognized Record Type values MUST be silently ignored. */
		break;
	}

	record = (struct igmpv3_grec *)((uint8_t *)record + record_size);
    }
}

/*
 * Calculate group membership timeout
 * Note: same as "Older Host Present Interval", RFC3376:8.13
 */
static uint32_t igmp_group_membership_timeout(void)
{
    return IGMP_ROBUSTNESS_VARIABLE * igmp_query_interval + IGMP_QUERY_RESPONSE_INTERVAL;
}

/*
 * Time out record of a group membership on a vif
 */
static void DelVif(void *arg)
{
    struct listaddr **anp;
    struct listaddr *group, *g;
    struct uvif *v;
    vifi_t vifi;
    cbk_t *cbk;

    cbk = (cbk_t *)arg;
    if (!arg)
	return;

    group = cbk->g;
    vifi = cbk->vifi;
    if (vifi >= MAXVIFS)
	return;

    v = &uvifs[vifi];

    if (IN_PIM_SSM_RANGE(group->al_addr)) {
	struct listaddr *curr, *prev = NULL;

	for (curr = group->al_sources; curr; prev = curr, curr = curr->al_next) {
	    inet_fmt(cbk->source, s1, sizeof(s1));
	    inet_fmt(curr->al_addr, s2, sizeof(s2));
	    IF_DEBUG(DEBUG_IGMP)
		logit(LOG_DEBUG, 0, "DelVif: Seek source %s, curr=%s (%p)", s1, s2, curr);

	    if (curr->al_addr == cbk->source) {
		if (!prev)
		    group->al_sources = curr->al_next; /* Remove from beginning */
		else
		    prev->al_next = curr->al_next;

		/* Stop any switch_version() timer */
		timer_clear(curr->al_versiontimer);

		free(curr);
		break;
	    }
	}

	IF_DEBUG(DEBUG_IGMP)
	    logit(LOG_DEBUG, 0, "DelVif: %s sources left", group->al_sources ? "Still" : "No");
	if (group->al_sources) {
	    IF_DEBUG(DEBUG_IGMP)
		logit(LOG_DEBUG, 0, "DelVif: Not last source, g->al_sources --> %s",
		      inet_fmt(group->al_sources->al_addr, s1, sizeof(s1)));
	    delete_leaf(vifi, cbk->source, group->al_addr);
	    free(cbk);

	    return;    /* This was not last source for this interface */
	}
    }

    /*
     * Group has expired
     * delete all kernel cache entries with this group
     */
    if (group->al_query)
	DeleteTimer(group->al_query);

    if (group->al_versiontimer)
	DeleteTimer(group->al_versiontimer);

    if (IN_PIM_SSM_RANGE(group->al_addr)) {
	inet_fmt(group->al_addr, s1, sizeof(s1));
	inet_fmt(cbk->source, s2, sizeof(s2));
	IF_DEBUG(DEBUG_IGMP)
	    logit(LOG_DEBUG, 0, "SSM range, source specific delete");

	/* delete (S,G) entry */
	IF_DEBUG(DEBUG_IGMP)
	    logit(LOG_DEBUG, 0, "DelVif: vif:%d(%s), (S=%s,G=%s)", vifi, v->uv_name, s2, s1);
	delete_leaf(vifi, cbk->source, group->al_addr);
    } else {
	delete_leaf(vifi, INADDR_ANY_N, group->al_addr);
    }

    anp = &v->uv_groups;
    while ((g = *anp)) {
	if (g == group) {
	    *anp = g->al_next;
	    free(g->al_sources);
	    free(g);
	} else {
	    anp = &g->al_next;
	}
    }

    free(cbk);
}

/*
 * Time out old version compatibility mode
 */
static void switch_version(void *arg)
{
    cbk_t *cbk = (cbk_t *)arg;

    if (cbk->g->al_pv < 3)
	cbk->g->al_pv += 1;

    logit(LOG_INFO, 0, "Switch IGMP compatibility mode back to v%d for group %s",
	  cbk->g->al_pv, inet_fmt(cbk->g->al_addr, s1, sizeof(s1)));

    free(cbk);
}

/*
 * Set a timer to switch version back on a vif.
 */
static int SetVerTimer(vifi_t vifi, struct listaddr *g)
{
    cbk_t *cbk;

    cbk = calloc(1, sizeof(cbk_t));
    if (!cbk) {
	logit(LOG_ERR, 0, "Failed calloc() in SetVerTimer()\n");
	return -1;
    }

    cbk->vifi = vifi;
    cbk->g = g;

    return timer_set(igmp_group_membership_timeout(), switch_version, cbk);
}

/*
 * Set a timer to delete the record of a group membership on a vif.
 */
static int SetTimer(vifi_t vifi, struct listaddr *g, uint32_t source)
{
    cbk_t *cbk;

    cbk = calloc(1, sizeof(cbk_t));
    if (!cbk) {
	logit(LOG_ERR, 0, "Failed calloc() in SetTimer()");
	return -1;
    }

    cbk->vifi = vifi;
    cbk->g = g;
    cbk->source = source;

    IF_DEBUG(DEBUG_IGMP)
	logit(LOG_DEBUG, 0, "Set delete timer for group: %s", inet_ntoa(*((struct in_addr *)&g->al_addr)));

    return timer_set(g->al_timer, DelVif, cbk);
}


/*
 * Delete a timer that was set above.
 */
static int DeleteTimer(int id)
{
    timer_clear(id);

    return 0;
}

/*
 * Send a group-specific query.
 */
static void SendQuery(void *arg)
{
    cbk_t *cbk = (cbk_t *)arg;
    struct uvif *v;

    v = &uvifs[cbk->vifi];

    if (v->uv_flags & VIFF_QUERIER) {
	uint32_t group = cbk->g->al_addr;

	IF_DEBUG(DEBUG_IGMP)
	    logit(LOG_DEBUG, 0, "SendQuery: Send IGMP v%s query", cbk->q_len == 4 ? "3" : "2");

	send_igmp(igmp_send_buf, v->uv_lcl_addr, group, IGMP_MEMBERSHIP_QUERY,
		  cbk->q_time, group != allhosts_group ? group : 0, cbk->q_len);
    }

    cbk->g->al_query = 0;
    free(cbk);
}


/*
 * Set a timer to send a group-specific query.
 */
static int SetQueryTimer(struct listaddr *g, vifi_t vifi, int to_expire, int q_time, int q_len)
{
    cbk_t *cbk;

    cbk = calloc(1, sizeof(cbk_t));
    if (!cbk) {
	logit(LOG_ERR, 0, "Failed calloc() in SetQueryTimer()");
	return -1;
    }

    cbk->g = g;
    cbk->q_time = q_time;
    cbk->q_len = q_len;
    cbk->vifi = vifi;

    return timer_set(to_expire, SendQuery, cbk);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "cc-mode"
 * End:
 */

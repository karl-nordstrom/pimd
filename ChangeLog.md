[v3.0.0][UNRELEASED]
--------------------

As of this release pimd is not guaranteed to be backwards compatible
with earlier releases.  It is recommended to run the same version of
pimd on all routers in the same domain.  See issue #93 for details.

**Note:** command line arguments in v3.0 are not compatible with v2.x!

### Changes
- Converted to GNU Configure & Build system
- Replace libite (`-lite`) GIT submodule with compatibility functions.
  I.e., as of this release `pimd` is self-hosting again.
- PID file is touched to acknowledge `SIGHUP`
- Completely changed command line arguments, in part to alert users that
  this release may not be compatible with their existing PIM routers
- New pimctl(8) tool completely replaces `pimd -r` and SIGUSR1 support,
  pimd no longer dumps internal state in a pimd.dump file
- Add `-i IDENT` to change name of `pimd` in syslog, `.conf` file, and
  PID file. Useful when running multiple daemons, one per multicast
  routing table
- Converted to use `getifaddrs()` when scanning for interfaces. This is
  required on at least FreeBSD 9, since `SIOCGIFCONF` seems to be really
  broken there, at least on systems with lots of interfaces
  (>30). Problem repored by Vladimir Shunkov
- Issue #41: Added systemd unit file
- Issue #56: Support for `disable-vifs` in `pimd.conf`
- Issue #62: Modify Join/Prune send logic to allow more than ~75
  mcast receivers, by Joonas Ruohonen
- Issue #67: Stream optimization, by Mika Joutsenvirta:
  - Switch directly to shortest path only in the case of PIM-SSM
  - For new IGMP group reports, send PIM Join immediately, saves 0-5 sec
  - Forward PIM Join messages immediately
- Issue #89: Allow use of loopback interface as long as MULTICAST
  flag is set, by Vincent Bernat
- Revert changes made for issue #66 and #73, no more implicit behavior
  of `bsr-candidate` and `rp-candidate`.  The case of no .conf file
  or commented out settings for the same, are now similar.  The default
  is now _disabled_

### Fixes
- Remove GNU:isms like `%m` and `__progname` to be able to build on
  systems that don't have them, like musl libc in e.g. Alpine Linux
- Issue #38: Allow enable `phyint` based on ifname or address.
- Issue #68: Do not enumerate VIFs for disabled interfaces. This fix
  allows using `pimd` on systems with more than 32 interfaces.
- Issue #84: Netlink code warns about missing VIF for disabled
  interfaces
- Issue #92: Fix RedHat/CentOS `.spec` file, building from GIT and
  from released tarball, by Sjoerd Boomstra
- Issue #93: Fixes a serious issue with the RP hash algorithm used in RP
  elections. The change makes `pimd` compatible with Cisco IOS, but it
  also makes `pimd` v3.0 *incompatible* with earlier `pimd`
  releases. Found and fixed by Xiaodong Xu


[v2.3.2][] - 2016-03-10
-----------------------

Bug fix release. All users should upgrade, in particular FreeBSD users!

### Changes
- Minor code cleanup and readability changes to simplify the code.
- Update to libite v1.4.2 with improved `min()`/`max()` macros
- Use `-Wextra` not `-Werror` in default `CFLAGS`, this to ensure that
  pimd still builds OK on newer and more pedantic compilers
- Update man page and example `pimd.conf` with details on `rp-candidate`
  `bsr-candidate`, two very important settings for correct operation.

### Fixes
- Issue #57: Multicast routing table not updated on FreeBSD.  Introduced
  with issue #23, in pimd v2.2.0.  Too intrusive changes altered
  handling (forwarding) of PIM register messages.  This only affects BSD
  systems, in particular FreeBSD 10.2 (current), or any FreeBSD < 11.0
- Issue #63: Mika Joutsenvirta <mika.joutsenvirta@insta.fi> found and
  fixed serious issues with the PIM Assert timeout handling.
- Issue #65: Missing slash in config file path when using env. variable
- Issue #66: Make it possible to run `pimd` without a configuration
  file. If `pimd` cannot find its configuration file it will use
  built-in fallback settings for `bsr-candidate` and `rp-candidate`.
  This to ensure you do not end up with a non-working setup.  To disable
  `bsr-candidate` and `rp-candidate`, simply leave them out of your
  config file, and make sure `pimd` can find the file.
- Issue #69: Rate limit only what is actually logged. The `logit()`
  function counted filtered messages, causing long periods of silence
  for no reason. Fix by Apollon Oikonomopoulos <apollon@skroutz.gr>


[v2.3.1][] - 2015-11-15
-----------------------

Bug fix release.

### Changes
- Let build system handle missing libite GIT submodule
- Issue #61: Debian packaging moved to https://github.com/bobek/pkg-pimd

### Fixes
- Issue #53: Build problem with Clang on FreeBSD
- Issue #55: Default config uses `/etcpimd.conf` instead of
  `/etc/pimd.conf`. Slashes added and now `pimd -h` lists the default
  path instead of a hard coded string.
- Issue #60: Fix minor spelling errors.


[v2.3.0][] - 2015-07-31
-----------------------

*The PIM-SSM & IGMPv3 release!*

The significant new features in this release were made possible thanks
to the hard work of Markus Veranen <markus.veranen@gmail.com> and Mika
Joutsenvirta <mika.joutsenvirta@insta.fi>.

Tested on Ubuntu 14.04 (GLIBC/Linux 3.13), Debian 8.1 (GLIBC/Linux
3.16), FreeBSD, NetBSD, and OpenBSD.

### Changes and New Features
- Support for PIM-SSM and IGMPv3, by Markus Veranen
- IGMPv3 is now default, use `phyint ifname igmpv2` for old behaviour
- Default IGMP query interval has changed from 125 sec to 12 sec

  In `pimd.conf: igmp-query-interval <SEC>`

- Default IGMP querier timeout has changed from 255 sec to 42 sec

  In `pimd.conf: igmp-querier-timeout <SEC>`

- The built-in IGMP *robustness value* changed from 2 to 3
- Support for changing the PIM Hello interval, by Markus Veranen

  In `pimd.conf: hello-interval <SEC>`

- Support for multiple multicast routing tables, and running multiple
  pimd instances, by Markus Veranen. (Only supported on Linux atm.)
- Support for advertising, and acting upon changes to, Generation ID
  in PIM Hello messages, by Markus Veranen
- Support for advertising *DR Priority* option in PIM Hello messages.
  If all routers on a LAN send this option this value is used in the
  DR election rather than the IP address. The priority is configured
  per `phyint`. This closes the long-standing issue #5.
- Distribution archive format changed from XZ to Gzip, for the benefit
  of OpenBSD that only ships Gzip in the base system.

### New pimd.conf syntax!

The `pimd.conf` syntax has been changed in this release. Mainly, the
configuration file now use dashes `-` instead of underscore `_` as word
separators. However several settings have also been renamed to be more
familiar to commands used by major router vendors:

- `bsr-candidate`: replaces `cand_bootstrap_router`
- `rp-candidate`: replaces `cand_rp`
- `group-prefix`: replaces `group_prefix`
- `rp-address`: replaces `rp_address`
- `spt-threshold`: replaces the `switch_register_threshold` and
  `switch_data_threshold` settings
- `hello-interval`: replaces `hello_period`
- `default-route-distance`: replaces `default_source_preference`
- `default-route-metric`: replaces `default-source-metric`

Also, for `phyint` the `preference` sub-option has been replaced with
the less confusing `distance` and `ttl-threshold` replaces `threshold`.
See the README or the man page for more information on the metric
preference and admin distance confusion.

> **Note:** The `pimd.conf` parser remains backwards compatible with the
>           old syntax!

### Compile Time Features

The following are new features that must be enabled at compile time,
using the `configure` script, to take effect.  For details, see
`./configure --help`

- `--prefix=PATH`: Standard prefix to be used at installation, default
  `/usr/local`
- `--sysconfdir=PATH`: Prefix path to be used for `pimd.conf`, default
  `/etc`, unless `--prefix` is given.
- `--embedded-libc`: Enable uClib or musl libc build, on Linux.
- `--disable-exit-on-error`: Allow pimd to continue running despite
  encountering errors.
- `--disable-pim-genid`: Disable advertisement of PIM Hello GenID, use
    for compatibility problems with older versions of pimd.
- `--with-max-vifs=MAXVIFS`: Raise max number of VIFs to MAXVIFS.  
    **Note:** this requires raising MAXVIFS in the kernel as well!  Most
    kernels cannot handle >255, if this is a problem, try using multiple
    multicast routing tables instead.
- `--disable-masklen-check`: Allow tunctl VIFs with masklen 32.

### Fixes
- Fix issue #40: FTBS with `./configure --enable-scoped-acls`
- Properly support cross compiling. It is now possible to actually
  define the `$CROSS` environment variable when calling `make` to
  allow cross compiling pimd. Should work with both GCC and Clang.
  Tested on Ubuntu, Debian and FreeBSD.


[v2.2.1][] - 2015-04-20
-----------------------

### Fixes
- Fix another problem with issue #22 (reopened), as laid out in issue
  #37.  This time the crash is induced when there is a link down event.
  Lot of help debugging the propblem by @mfspeer, who also suggested
  the fix -- to call `pim_proto.c:delete_pim_nbr()` in
  `vif.c:stop_vif()` instead of just calling free.
- Fix issue with not checking return value of `open()` in daemonizing
  code in `main()`, found by Coverity Scan.
- Fix issue with scoped `phyint` in `config.c`, found by Coverity
  Scan. `masklen` may not be zero, config file problem, alert user.


[v2.2.0][] - 2014-12-28
-----------------------

### Changes
- Support for IP fragmentation of PIM register messages, by Michael Fine,
  Cumulus Networks
- Support `/LEN` syntax in `phyint`, complement `masklen LEN`, issue #12
- Support for /31 networks, point-to-point, by Apollon Oikonomopoulos
- Remove old broken SNMP support
- OpenBSD inspired cleanup (deregister)
- General code cleanup, shorten local variable names, func decl. etc.
- Support for router alert IP option in IGMP queries
- Support for reading IGMPv3 membership reports
- Update IGMP code to support FreeBSD >= 8.x
- Retry read of routing tables on FreeBSD
- Fix join/leve of ALL PIM Routers for FreeBSD and other UNIX kernels
- Tested on FreeBSD, NetBSD and OpenBSD
- Add very simple homegrown configure script
- Update and document support for `rp_address`, `cand_rp`, and
  `cand_bootstrap_router`
- Add new `spt_threshold` to replace existing
  `switch_register_threshold` and `switch_data_threshold settings`.
  Cisco-like and easier to understand

### Fixes
- Avoid infinite loop during unicast send failure, by Alex Tessmer
- Fix bug in bootstrap when configured as candidate RP, issue #15
- Fix segfault in `accept_igmp()`, issue #29
- Fix default source preference, should be 101 (not 1024!)
- Fix issue #23: `ip_len` handling on older BSD's, thanks to Olivier
  Cochard-Labbé
- Fix default prefix len in static RP example in `pimd.conf`
- Fix issue #31: Make IGMP query interval and querier timeout configurable
- Fix issue #33: pimd does not work in background under FreeBSD
- Fix issue #35: support for timing out other queriers from mrouted
- Hopefully fix issue #22: Crash in (S,G) state when neighbor is lost
- Misc. bug fixes thanks to Coverity Scan, static code analysis tool
  <https://scan.coverity.com/projects/3319>


[v2.1.8][] - 2011-10-22
-----------------------

### Changes
- Update docs of static Rendez-Vous Point, `rp_address`, configuration
  in man page and example `pimd.conf`.  Thanks to Andriy Senkovych
  <andriysenkovych@gmail.com> and YAMAMOTO Shigeru <shigeru@iij.ad.jp>
- Replaced `malloc()` with `calloc()` to mitigate risk of accessing junk
  data and ease debugging, by YAMAMOTO Shigeru <shigeru@iij.ad.jp>
- Extend .conf file `rp_address` option with `priority` field.  Code
  changes and doc updates by YAMAMOTO Shigeru <shigeru@iij.ad.jp>

### Fixes
- A serious bug in `pim_proto.c:receive_pim_register()` was found and
  fixed by Jean-Pascal Billaud.  In essence, the RP check was broken
  since the code only looked at `my_cand_rp_address`, which is not set
  when using the `rp_address` config.  Everything works fine with
  auto-RP mode though.  This issue completely breaks the register path
  since the JOIN(S,G) is never sent back ...
- Fix FTBFS issues reported from Debian.  Later GCC versions trigger
  unused variable warnings.  Fixes by Antonin Kral <a.kral@bobek.cz>


[v2.1.7][] - 2011-01-09
-----------------------

### Changes
- The previous move of runtime dump files to `/var/lib/misc` have been
  changed to `/var/run/pimd` instead.  This to accomodate *BSD systems
  that do not have the `/var/lib` tree, and also recommended in the
  Filesystem Hierarchy Standard:

  <http://www.pathname.com/fhs/pub/fhs-2.3.html#VARRUNRUNTIMEVARIABLEDATA>


[v2.1.6][] - 2011-01-08
-----------------------

### Changes
- Debian package now conflicts with `smcroute`, in addition to `mrouted`.
  It is only possible to run one multicast routing daemon at a time, kernel
  limitation.
- The location of the dump file(s) have been moved from `/var/tmp` to
  `/var/lib/misc` due to the insecure nature of `/var/tmp`.  See more
  below.

### Fixes
- `kern.c:k_del_vif()`: Fix build error on GNU/kFreeBSD
- CVE-2011-0007: Insecure file creation in `/var/tmp`. "On USR1, pimd
  will write to `/var/tmp/pimd.dump` a dump of the multicast route
  table.  Since `/var/tmp` is writable by any user, a user can create a
  symlink to any file he wants to destroy with the content of the
  multicast routing table."


[v2.1.5][] - 2010-11-21
-----------------------

### Changes
- Improved error messages in kern.c
- Renamed CHANGES to ChangeLog

### Fixes
- Import mrouted fix: on GNU/Linux systems (only!) the call to
  `kern.c:k_del_vif()` fails with: `setsockopt MRT_DEL_VIF on vif 3:
  Invalid argument`.  This is due to differences in the Linux and *BSD
  `MRT_DEL_VIF` API.  The Linux kernel expects to receive a `struct
  vifctl` associated with the VIF to be deleted, *BSD systems on the
  other hand expect to receive the index of that VIF.

  Bug reported and fixed on mrouted by Dan Kruchinin <dkruchinin@acm.org>


[v2.1.4][] - 2010-09-25
-----------------------

### Changes
- Support Debian GNU/kFreeBSD, FreeBSD kernel with GNU userland.

### Fixes
- Lior Dotan <liodot@gmail.com> reports that pimd 2.1.2 and 2.1.3 are
  severely broken w.r.t.  uninformed systematic replace of `bcopy()`
  with `memcpy()` API.


[v2.1.3][] - 2010-09-08
-----------------------

### Changes
- `debug.c:syslog()`: Removed GNU:ism %m, use `strerror(errno)` instead.
- Cleanup and ansification of: rp.c, mrt.c, vif.c, route.c
- Initialize stack variables to silence overzealous GCC on PowerPC and
  S/390. Debian bug 595584, this closes pimd issue #3 on GitHub.

### Fixes
- Bug fix for static-rp configurations from Kame's pim6sd route.c r1.28
- Close TODO, merge in relevant changes from Kame's pim6sd `vif.c r1.3`
- Tried fixing `debug.c:logit()` build failure on Sparc due to mixup
  in headers for `tv_usec` type.


[v2.1.2][] - 2010-09-04
-----------------------

### Changes
- License change on mrouted code from OpenBSD team => pimd fully free
  under the simlified 3-clause BSD license!  This was also covered in
  v2.1.0-alpha29.17, but now all files have been updated, including
  LICENSE.mrouted.
- Code cleanup and ansification.
- Simplify Makefile so it works seamlessly on GNU Make and BSD PMake.
- Replaced `bzero()` and `bcopy()` with `memset()` and `memcpy()`.
- Use `getopt_long()` for argument parsing.
- Add, and improve, `-h,--help` output.
- Add `-f,--foreground` option.
- Add `-v,--version` option.
- Add `-l,--reload-config` which sends SIGHUP to a running daemon.
- Add `-r,--show-routes` which sends SIGUSR1 to a running daemon.
- Add `-q,--quit-daemon` which sends SIGTERM to a running daemon.
- Enable calling pimd as a regular user, for `--help` and `--version`.
- Man page cleaned up, a lot, and updated with new options.

### Fixes
- Replaced dangerous string functions with `snprintf()` and `strlcpy()`
- Added checks for `malloc()` return values, all over the code base.
- Fixed issues reported by Sparse (CC=cgcc).
- Retry syscalls `recvfrom()` and `sendto()` on signal (SIGINT).
- Fix build issues on OpenBSD 4.7 and FreeBSD 8.1, Guillaume Sellier.
- Kernel include issues on Ubuntu 8.04, Linux <= 2.6.25, Nikola Knežević
- Fix build issues on NetBSD


[v2.1.1][] - 2010-01-17
-----------------------

Merged all patches from <http://lintrack.org>.

### Changes
- Bumping version again to celebrate the changes and make it easier
  for distributions to handle the upgrade.
- `002-better-rp_address.diff`: Support multicast group address in
  static Rendez-Vous Point .conf option.
- `004-disableall.diff`: Add -N option to pimd.
- `005-vifenable.diff`: Add enable keyword to phyint .conf option.

### Fixes
- `001-debian-6.diff`: Already merged, no-op - only documenting in case
  anyone wonders about it.
- `003-ltfixes.diff`: Various bug fixes and error handling improvements.
- `006-dot19.diff`: The lost alpha29.18 and alpha29.19 fixes by Pavlin
  Radoslavov.


[v2.1.0][] - 2010-01-16
-----------------------

First release from new home at GitHub and by new maintainer.

### Changes
- Integrated the Debian patches from `pimd_2.1.0-alpha29.17-9.diff.gz`
- Fixed the new file include/linux/netinet/in-my.h (Debian) so that the
  #else fallback uses the system netinet/in.h, which seems to work now.
- Bumped version number, this code has been available for a while now.


v2.1.0-alpha29.19 - 2005-01-14
------------------------------

### Fixes
- Don't ignore PIM Null Register messages if the IP version of the inner
  header is not valid.
- Add a missing bracket inside rsrr.c (a bug report and a fix by
  <seyon@oullim.co.kr>)


v2.1.0-alpha29.18 - 2003-05-21
------------------------------

### Changes
- Compilation fix for Solaris 8. Though, no guarantee pimd still works
  on that platform.
- Define `BYTE_ORDER` if missing.
- Update include/netinet/pim.h file with its lastest version
- Update the copyright message of `include/netinet/pim_var.h`


v2.1.0-alpha29.17 - 2003-03-20
------------------------------

### Changes
- The mrouted license, LICENSE.mrouted, updated with BSD-like license!!
  Thanks to the OpenBSD folks for the 2 years of hard work to make this
  happen:

  <http://www.openbsd.org/cgi-bin/cvsweb/src/usr.sbin/mrouted/LICENSE>

- Moved the pimd contact email address upfront in README. Let me repeat
  that here: If you have any questions, suggestions, bug reports, etc.,
  do NOT send them to the PIM IETF Working Group mailing list! Instead,
  use the contact email address specified in README.


v2.1.0-alpha29.16 - 2003-02-18
------------------------------

### Fixes
-   Compilation bugfix for Linux. Bug report by Serdar Uezuemcue
    <serdar@eikon.tum.de>


v2.1.0-alpha29.15 - 2003-02-12
------------------------------

### Fixes
- Routing socket descriptor leak. Bug report and fix by SUZUKI Shinsuke
  <suz@crl.hitachi.co.jp>, incorporated back from pim6sd.
- PIM join does not go upstream. Bug report and fix by SUZUKI Shinsuke
  <suz@crl.hitachi.co.jp>, incorporated back from pim6sd.

``` {.example}
[problem]
PIM join does not go upstream in the following topology, because oif-list
is NULL after subtracting iif from oif-list.

    receiver---rtr1---|
               rtr2---|---rtr3----sender

            rtr1's nexthop to sender = rtr2
            rtr2's nexthop to sender = rtr3

[reason]
Owing to a difference between RFC2362 and the new pim-sm draft.
[solution]
Prunes iif from oiflist when installing it into kernel, instead of
PIM route calculation time.
```


v2.1.0-alpha29.14 - 2003-02-10
------------------------------

### Fixes
- Bugfix in calculating the netmask for POINTOPOINT interface in
  config.c. Bug report by J.W. (Bill) Atwood <bill@cs.concordia.ca>
- `rp.c:rp_grp_match()`: SERIOUS bugfix in calculating the RP per
  group when there are a number of group prefixes in the Cand-RP set.
  Bug report by Eva Pless <eva.pless@imk.fraunhofer.de>


v2.1.0-alpha29.13 - 2002-11-07
------------------------------

### Fixes
- Bugfix in rp.c `bootstrap_initial_delay()` in calculating BSR
  election delay. Fix by SAKAI Hiroaki <sakai.hiroaki@finet.fujitsu.com>


v2.1.0-alpha29.12 - 2002-10-26
------------------------------

### Fixes
- Increase size of send buffers in the kernel.  Bug report by Andrea
  Gambirasio <andrea.gambirasio@softsolutions.it>


v2.1.0-alpha29.11 - 2002-07-08
------------------------------

### Fixes
Bug reports and fixes by SAKAI Hiroaki <sakai.hiroaki@finet.fujitsu.com>

- `init_routesock()`: Bugfix: initializing a forgotten variable. The
  particular code related to that variable is commented-out by default,
  but a bug is a bug.
- `main.c:restart()`: Bugfix: close the `udp_socket` only when it is
  is different from `igmp_socket`.
- `main.c:main()`: if SIGHUP signal is received, reconstruct readers
  and nfds
- Three serious bug fixes thanks to Jiahao Wang <jiahaow@yahoo.com.cn>
  and Bo Cheng <bobobocheng@hotmail.com>:
  - `pim_proto.c:receive_pim_join_prune()`: two bugfixes related to
    the processing of `(*,*,RP)`
  - `pim_proto.c:add_jp_entry()`: Bugfix regarding adding prune
    entries
- Remove the FTP URL from the various README files, and replace it
  with an HTTP URL, because the FTP server on catarina.usc.edu is not
  operational anymore.


v2.1.0-alpha29.10 - 2002-04-26
------------------------------

### Fixes
- Widen space for "Subnet" addresses printed in "Virtual Interface Table"
- Added (commented-out code) to enable different interfaces to belong
  to overlapping subnets. See around line 200 in config.c
- Bugfix in handling of Join/Prune messages when there is one join and one
  prune for the same group. Thanks to Xiaofeng Liu <liu_xiao_feng@yahoo.com>


v2.1.0-alpha29.9 - 2001-11-13
-----------------------------

### Changes

First three entries contributed by Hiroyuki Komatsu <komatsu@taiyaki.org>

- Print line number if there is conf file error.
- If there is an error in the conf file, pimd won't start.
- GRE configuration examples added to README.config.
- New file README.debug (still very short though).

### Fixes
- Increase the config line buffer size to 1024. Bug fix by Hiroyuki
  Komatsu <komatsu@taiyaki.org>


v2.1.0-alpha29.8 - 2001-10-16
-----------------------------

### Changes
- Better log messages for point-to-point links in config.c. Thanks to
  Hitoshi Asaeda <asaeda@yamato.ibm.com>


v2.1.0-alpha29.7 - 2001-10-10
-----------------------------

### Changes
- Added `phyint altnet` (see pimd.conf for usage) for allowing some
  senders look like directly connected to a local subnet. Implemented by
  Marian Stagarescu <marian@bile.cidera.com>
- Added `phyint scoped` (see pimd.conf for usage) for administratively
  disabling the forwarding of multicast groups.  Implemented by Marian
  Stagarescu <marian@bile.cidera.com>
- The License has changed from the original USC to the more familiar
  BSD-like (the KAME+OpenBSD guys brought to my attention that the
  original working in the USC license "...and without fee..." is
  ambiguous and makes it sound that noone can distribute pimd as part of
  some other software distribution and charge for that distribution.
- RSRR disabled by default in Makefile

### Fixes
- Memory leaks bugs fixed in rp.c, thanks to Sri V <vallepal@yahoo.com>
- Compilation problems for RedHat-7.1 fixed. Bug report by Philip Ho
  <cbho@ie.cuhk.edu.hk>
- PID computation fixed (it should be recomputed after a child
  `fork()`).  Thanks to Marian Stagarescu <marian@bile.cidera.com>
- `find_route()`-related bug fixes (always explicitly check for NULL
  return). Bug report by Marian Stagarescu <marian@bile.cidera.com>
- Bug fix re. adding a local member with older Ciscos in `add_leaf()`.
  Bug report by Marian Stagarescu <marian@bile.cidera.com>
- Added explicit check whether `BYTE_ORDER` in pimd.h is defined. Bug
  report by <mistkhan@indiatimes.com>


v2.1.0-alpha29.6 - 2001-05-04
-----------------------------

### Fixes
- Bug fixes in processing Join/Prune messages.  Thanks to Sri V 
 <vallepal@yahoo.com>


v2.1.0-alpha29.5 - 2001-02-22
-----------------------------

### Changes
- `VIFM_FORWARDER()` macro renamed to `VIFM_LASTHOP_ROUTER`.
- Mini-FAQ entries added to README.

### Fixes
- When there is a new member, `add_leaf()` is called by IGMP code for
  any router, not only for a DR.  The reason is because not only the DR
  must know about local members, but the last-hop router as well (so
  eventually it will initiate a SPT switch). Similar fixes to
  `add_leaf()` inside route.c as well. Problem reported by Hitoshi
  Asaeda <asaeda@yamato.ibm.com>. XXX: Note the lenghty comment in the
  beginning of `add_leaf()` about a pimd desing problem that may result
  in SPT switch not initiated immediately by the last-hop router.
- DR entry timer bug fix in timer.c: When `(*,G)`'s iif and (S,G)'s iif
  are not same, (S,G)'s timer for the DR doesn't increase.  Reported
  indirectly by <toshiaki.nakatsu@fujixerox.co.jp>


v2.1.0-alpha29.4 - 2000-12-01
-----------------------------

### Changes
- README cleanup + Mini-FAQ added
- `igmp_proto.c`: printf argument cleanup (courtesy KAME)
- `main.c:restart()`: forgotten printf argument added (courtesy KAME)

### Fixes
- `kern.c:k_stop_pim()`: Fix the ordering of `MRT_PIM` and `MRT_DONE`,
  thanks to Hitoshi Asaeda <asaeda@yamato.ibm.co.jp>
- `route.c:add_leaf()`: mrtentry creation logic bug fix. If the router
  is not a DR, a mrtentry is never created. Tanks to Hitoshi Asaeda
  <asaeda@yamato.ibm.co.jp> & (indirectly) <toshiaki.nakatsu@fujixerox.co.jp>
- `pim_proto.c`: Two critical bug fixes. J/P prune suppression related
  message and J/P message with `(*,*,RP)` entry inside. Thanks to
  Azzurra Pantella <s198804@studenti.ing.unipi.it> and Nicola Dicosmo
  from University of Pisa
- `pim_proto.c:receive_pim_bootstrap()`: BSR-related fix from Kame's
  pim6sd.  Even when the BSR changes, just schedule an immediate
  advertisemnet of C-RP-ADV, instead of sending message, in order to
  avoid sending the advertisement to the old BSR.  In response to comment
  from <toshiaki.nakatsu@fujixerox.co.jp>


v2.1.0-alpha29.3 - 2000-10-13
-----------------------------

### Fixes
- `ADVANCE()` bug fix in routesock.c (if your system doesn't have
  `SA_LEN`) thanks to Eric S. Johnson <esj@cs.fiu.edu>


v2.1.0-alpha29.2 - 2000-10-13
-----------------------------

NB: THIS pimd VERSION WON'T WORK WITH OLDER PIM-SM KERNEL PATCHES
(kernel patches released prior to this version)!

### Changes
- The daemon that the kernel will prepare completely the inner multicast
  packet for PIM register messages that the kernel is supposed to
  encapsulate and send to the RP.
- Now pimd compiles on OpenBSD-2.7. PIM control messages exchange test
  passed.  Don't have the infrastructure to perform more complete testing.
- `main.c:cleanup()`: Send `PIM_HELLO` with holdtime of '0' if pimd
  is going away, thanks to JINMEI Tatuya <jinmei@isl.rdc.toshiba.co.jp>
- `include/netinet/pim.h` updated
- pimd code adapted to the new `struct pim` definition.
- Added `PIM_OLD_KERNEL` and `BROKEN_CISCO_CHECKSUM` entries in the
  Makefile.
- Don't ignore kernel signals if any of src or dst are NULL.
- Don't touch `ip_id` on a PIM register message
- README cleanup: kernel patches location, obsolete systems clarification, etc.
- `k_stop_pim()` added to `cleanup()` in `main.c` (courtesy Kame)

### Fixes
- `RANDOM()` related bug fix re. `jp_value` calculation in `pim_proto.c`,
  thanks to JINMEI Tatuya <jinmei@isl.rdc.toshiba.co.jp>
- `realloc()` related memory leak bug in `config_vifs_from_kernel()`
  in config.c courtesy Kame's pim6sd code.
- Solaris-8 fixes thanks to Eric S. Johnson <esj@cs.fiu.edu>
- `BROKEN_CISCO_CHECKSUM` bug fix thanks to Eric S. Johnson
  <esj@cs.fiu.edu> and Hitoshi Asaeda.
- `main.c`: 1000000 usec -> 1 sec 0 usec.  Fix courtesy of the Kame Project
- `main.c:restart()` fixup courtesy of the Kame Project
- various min. message length check for the received control messages
  courtesy of the Kame project. XXX: the pimd check is not enough!
- VIF name string comparison fix in `routesock.c:getmsg()`, courtesy
  of the Kame project
- Missing brackets added inside `age_routes()`, shows up only if
  `KERNEL_MFC_WC_G` is defined, courtesy of the Kame Project


v2.1.0-alpha28 - 2000-05-15
---------------------------

### Changes
- added #ifdef `BROKEN_CISCO_CHECKSUM` (disabled by default) to make
  cisco RPs happy (read the comments in pim.c)
- added #ifdef `PIM_TYPEVERS_DECL` in netinet/pim.h as a workaround that
  ANSI-C doesn't guarantee that bit-fields are tightly packed together
  (although all modern C compilers should not create a problem).

### Fixes
- Fixes to enable point-to-point interfaces being added correctly,
  thanks to Roger Venning <Roger.Venning@corpmail.telstra.com.au>
- A number of minor bug fixes


v2.1.0-alpha27 - 2000-01-21
---------------------------

NB: this release may the the last one from 2.1.0.  The next release will
be 2.2.0 and there will be lots of changes inside.

### Fixes
- Bug fix in `rp.c:add_grp_mask()` and `rp.c:delete_grp_mask()`: in
  some cases if the RPs are configured with nested multicast prefixes,
  the add/delete may fail.  Thanks to Hitoshi Asaeda and the KAME team
  for pointing out this one.


v2.1.0-alpha26 - 1999-10-29
---------------------------

### Fixes

-   Bug fix in `receive_pim_register()` in `pim_proto.c:ntohl()` was
    missing inside `IN_MULTICAST()`. Thanks to Fred Griffoul
    <griffoul@ccrle.nec.de>

-   Bug report and fix by Hitoshi Asaeda <asaeda@yamato.ibm.co.jp> in
    `pim_proto.c:receive_pim_cand_rp_adv()` (if a router is not a BSR).
    Another bug in `rp.c:delete_grp_mask_entry()`: an entry not in the
    head of the list was not deleted propertly.

-   Some `VIFF_TUNNEL` checks added or deleted in various places. Slowly
    preparing pimd to be able to work with GRE tunnels...


v2.1.0-alpha25 - 1999-08-30
---------------------------

Bug reports and fixes by Hitoshi Asaeda <asaeda@yamato.ibm.co.jp> inside
`parse_reg_threshold()` and `parse_data_threshold()` in config.c

### Changes
- Successfully added multicast prefixes configured in pimd.conf are
  displayed at startup
- Use `include/freebsd` as FreeBSD-3.x include files and
  `include/freebsd2` for FreeBSD-2.x.

### Fixes
- Test is performed whether a `PIM_REGISTER` has invalid source and/or
  group address of the internal packet.


v2.1.0-alpha24 - 1999-08-09
---------------------------

### Changes
- `PIM_DEFAULT_CAND_RP_ADV_PERIOD` definition set to 60, but default
  time value for inter Cand-RP messages is set in pimd.conf to 30 sec.
- `PIM_REGISTER` checksum verification in `receive_pim_register()`
  relaxed for compatibility with some older routers. The checksum has
  to be computed only over the first 8 bytes of the PIM Register (i.e.
  only over the header), but some older routers might compute it over
  the whole packet. Hence, the checksum verification is over the first
  8 bytes first, and if if it fails, then over the whole packet. Thus,
  pimd that is RP should still work with older routers that act as DR,
  but if an older router is the RP, then pimd cannot be the DR. Sorry,
  don't know which particular routers and models create the checksum
  over the whole PIM Register (if there are still any left).


v2.1.0-alpha23 - 1999-05-24
---------------------------

### Changes
- Finally pimd works under Linux (probably 2.1.126, 2.2.x and 2.3.x).
  However, a small fix in the kernel `linux/net/ipv4/ipmr.c` is
  necessary. In function `pim_rcv()`, remove the call to
  `ip_compute_csum()`:

``` {.c}
--- linux/net/ipv4/ipmr.c.org   Thu Mar 25 09:23:34 1999
+++ linux/net/ipv4/ipmr.c       Mon May 24 15:42:45 1999
@@ -1342,8 +1342,7 @@
         if (len < sizeof(*pim) + sizeof(*encap) ||
            pim->type != ((PIM_VERSION<<4)|(PIM_REGISTER)) ||
            (pim->flags&PIM_NULL_REGISTER) ||
-           reg_dev == NULL ||
-           ip_compute_csum((void *)pim, len)) {
+           reg_dev == NULL) {
                kfree_skb(skb);
                 return -EINVAL;
         }
```

- in pimd.conf `phyint` can be specified not only by IP address, but
  by name too (e.g. `phyint de1 disable`)
- in pimd.conf `preference` and `metric` can be specified per `phyint`
  Note that these `preference` and `metric` are like per iif.
- `MRT_PIM` used (again) instead of `MRT_ASSERT` in kern.c.  The problem
  is that Linux has both `MRT_ASSERT` and `MRT_PIM`, while *BSD has only
  `MRT_ASSERT`.

``` {.c}
#ifndef MRT_PIM
#define MRT_PIM MRT_ASSERT
#endif
```

- Rely on `__bsdi__`, which is defined by the OS, instead of `-DBSDI` in
  Makefile, change by Hitoshi Asaeda.  Similarly, use `__FreeBSD__`
  instead of `-DFreeBSD`
- Linux patches by Fred Griffoul <griffoul@ccrle.nec.de> including a
  `netlink.c` instead of `routesock.c`
- `vif.c:zero_vif()`: New function

### Fixes
All bug reports thanks to Kaifu Wu <kaifu@3com.com>

- Linux-related bug fixes regarding raw IP packets byte ordering
- Join/Prune message bug fixed if the message contains several groups
  joined/pruned


v2.1.0-alpha22 - 1998-11-11
---------------------------

Bug reports by Jonathan Day <jd9812@my-dejanews.com>

### Fixes
- Bug fixes to compile under newer Linux kernel (linux-2.1.127) To
  compile for older kernels (ver < ???), add `-Dold_Linux` to the
  Makefile
- For convenience, the `include/linux/netinet/{in.h,mroute.h}` files
  are added, with few modifications applied.


v2.1.0-alpha21 - 1998-11-04
---------------------------

### Fixes
- `pim_proto.c:join_or_prune()`: Bug fixes in case of (S,G) overlapping
  with `(*,G)`. Bug report by Dirk Ooms <Dirk.Ooms@alcatel.be>
- `route.c:change_interfaces()`: Join/Prune `(*,G)`, `(*,*,RP)` fire
  timer optimization/fix.


v2.1.0-alpha20 - 1998-08-26
---------------------------

### Changes
- (Almost) all timers manipulation now use macros
- `pim.h` and `pim_var.h` are in separate common directory
- Added BSDI definition to `pim_var.h`, thanks to Hitoshi Asaeda.

### Fixes
- fix TIMEOUT definitions in difs.h (bug report by Nidhi Bhaskar)
  (originally, if timer value less than 5 seconds, it won't become 0)
  It is HIGHLY recommended to apply that fix, so here it is:

``` {.c}
-------------BEGIN BUG FIX-------------------
  1) Add the following lines to defs.h (after #define FALSE):

#ifndef MAX
#define MAX(a,b) (((a) >= (b))? (a) : (b))
#define MIN(a,b) (((a) <= (b))? (a) : (b))
#endif /* MAX & MIN */

  2) Change the listed below TIMEOUT macros to:

#define IF_TIMEOUT(timer)      \
    if (!((timer) -= (MIN(timer, TIMER_INTERVAL))))

#define IF_NOT_TIMEOUT(timer)      \
    if ((timer) -= (MIN(timer, TIMER_INTERVAL)))

#define TIMEOUT(timer)         \
    (!((timer) -= (MIN(timer, TIMER_INTERVAL))))

#define NOT_TIMEOUT(timer)     \
    ((timer) -= (MIN(timer, TIMER_INTERVAL)))
---------------END BUG FIX-------
```


v2.1.0-alpha19 - 1998-06-29
---------------------------

Both bug reports by Chirayu Shah <shahzad@torrentnet.com>

### Fixes
- bug fix in `find_route()` when searching for `(*,*,RP)`
- bug fix in `move_kernel_cache()`: no need to do `move_kernel_cache()`
  from `(*,*,R)` to `(*,G)` first when we call `move_kernel_cache()` for
  (S,G)


v2.1.0-alpha18 - 1998-05-29
---------------------------

### Changes
- Now compiles under Linux (haven't checked whether the PIMv2 kernel
  support in linux-2.1.103 works)

### Fixes
- `parse_default_source*()` bug fix (bug reports by Nidhi Bhaskar)
- allpimrouters deleted from igmp.c (already defined in pim.c)
- igmpmsg defined for IRIX


v2.1.0-alpha17 - 1998-05-21
---------------------------

### Changes
- `(*,G)` MFC kernel support completed and verified. Compile with
  `KERNEL_MFC_WC_G` defined in Makefile, but then must use it only
  with a kernel that supports `(*,G)`, e.g. `pimkern-PATCH_7`.
  Currently, kernel patches available for FreeBSD and SunOS only.

### Fixes
- `MRTF_MFC_CLONE_SG` flag set after `delete_single_kernel_cache()` is
  called


v2.1.0-alpha16 - 1998-05-19
---------------------------

### Changes
- PIM registers kernel encapsulation support. Build with
  `PIM_REG_KERNEL_ENCAP` defined in Makefile.
- `(*,G)` MFC support. Build with `KERNEL_MFC_WC_G` defined in
  Makefile. However, `MFC_WC_G` is still not supported with
  `pimkern-PATCH_6`, must disable it for now.
- `mrt.c:delete_single_kernel_cache_addr()`: New function, uses
  source, group to specify an MFC to be deleted


v2.1.0-alpha15 - 1998-05-14
---------------------------

- Another few bug fixes related to NetBSD definitions thanks to Heiko
  W.Rupp <hwr@pilhuhn.de>


v2.1.0-alpha14 - 1998-05-12
---------------------------

- A few bug fixes related to NetBSD definitions thanks to Heiko W.Rupp
  <hwr@pilhuhn.de>


v2.1.0-alpha13 - 1998-05-11
---------------------------

### Changes
- If the RP changes, the necessary actions are taken to pass the new RP
  address to the kernel.  To be used for kernel register encap support.
  Wnat needs to be done is: (a) add `rp_addr` entry to the mfcctl
  structure, and then just set it in `kern.c:k_chf_mfc()`.  Obviously,
  the kernel needs to support the register encapsulation (instead of
  sending WHOLEPKT to the user level).  In the near few days will make
  the necessary kernel changes.
- `change_interfaces()`: Added "flags" argument. The only valid flag
  is `MFC_UPDATE_FORCE`, used for forcing kernel call when only the RP
  changes.
- `k_chg_mfc()` has a new argument: rp~addr~. To be used for kernel
  register encapsulation support
- `MRT_PIM` completely replaced by `MRT_ASSERT`
- `move_kernel_cache()`: Argument `MFC_MOVE_FORCE` is a flag instead
  of TRUE/FALSE
- `process_cache_miss()`: removed unneeded piece of code


v2.1.0-alpha12 - 1998-05-10
---------------------------

### Changes
- Use the cleaned up `netinet/pim.h`
- Remove the no needed anymore pim header definition in `pimd.h`
- Don't use `MRT_PIM` in in kern.c anymore, replaced back with
  `MRT_ASSERT`.
- `added default_source_metric` and `default_source_preference` (1024)
  because the kernel's unicast routing table is not a good source of
  info; configurable in pimd.conf
- Can now compile under NetBSD-1.3, thanks to Heiko W.Rupp <hwr@pilhuhn.de>

### Fixes
- Incorrect setup of the borderBit and nullRegisterBit (different for
  big and little endian machines) fixed; `*_BORDER_BIT` and
  `*NULL_REGISTER_BIT` redefined
- don't send `pim_assert` on tunnels or register vifs (if for whatever
  reason we receive on such interface)
- ignore `WRONGVIF` messages for register and tunnel vifs (the cleaned
  up kernel mods dont send such signal, but the older (before May 9 '98)
  pimd mods that signaling was enabled


v2.1.0-alpha11 - 1998-03-16
---------------------------

### Changes
- `vif.c:find_vif_direct_local()`: New function, used in `routesock.c`,
  `igmp_proto.c`
- Use `MFC_MOVE_FORCE/MFC_MOVE_DONT_FORCE` flag in `mrt.c`, `route.c`,
  `pim_proto.c`, when need to move the kernel cache entries between
  `(*,*,RP)`, `(*,G)`, `(S,G)`
- new timer related macros: `SET_TIMER()`, `FIRE_TIMER()`,
  `IF_TIMER_SET()`, `IF_TIMER_NOT_SET()`

### Fixes
- `timer.c:age_routes()`: bunch of fixes regarding J/P message
  fragmentation
- `route.c:process_wrong_iif()`: (S,G) SPT switch bug fix: ANDed
  `MRTF_RP` fixed to `MRTF_RP`
- `pim_proto.c` & `timer.c`: (S,G) Prune now is sent toward RP, when
  iif toward S and iif toward RP are different
- `pim_proto.c:join_or_prune()` bug fixes
- `pim_proto.c`: (S,G) Prune entry's timer now set to J/P message
  holdtime
- `pim_proto.c:receive_pim_join_prune()`: Ensure pruned interfaces are
  correctly reestablished
- `timer.c:age_routes()`: now (S,G) entry with local members
  (inherited from `(*,G)`) is timeout propertly
- `timer.c:age_routes()`: (S,G) J/P timer restarted propertly
- `timer.c:age_routes()`: check also the (S,G)RPbit entries in the
  forwarders and RP and eventually switch to the shortest path if data
  rate too high
- `route.c:process_wrong_vif()` fire J/P timer
- `route.c:switch_shortest_path()`: reset the iif toward S if there is
  already (S,G)RPbit entry


v2.1.0-alpha10 - 1998-03-03
---------------------------

Temp. non-public release.

### Changes
- `interval` can be applied for data rate check. The statement in
  `pimd.conf` that only the default value will be used is not true
  anymore.
- The RP-initiated and the forwarder-initiated (S,G) switch threshold
  rate can be different.
- `pim_proto.c:receive_pim_register()`: check if I am the RP for that
  group, and if "no", send `PIM_REGISTER_STOP` (XXX: not in the
  spec, but should be!)
- `pim_proto.c:receive_pim_register_stop()`: check if the
  `PIM_REGISTER_STOP` originator is really the RP, before suppressing
  the sending of the PIM registers. (XXX: not in the spec but should
  be there)
- `rp.c:check_mrtentry_rp()`: new function added to check whether the
  RP address is the corresponding one for the given mrtentry
- `debug.c:dump_mrt()` timer values added
- `route.c`: `add_leaf()`, `process_cache_miss()`,
  `process_wrong_iif()` no routing entries created for the LAN scoped
  addresses
- `DEBUG_DVMRP_DETAIL` and `DEBUG_PIM_DETAIL` added

### Fixes
- `mrt.c:add_kernel_cache()`: no kernel cache duplicates
- `mrt.c:move_kernel_cache()`: if the iif of the `(*,*,R)` (or `(*,G)`)
  and (S,G) are different, dont move the cache entry "UP"
- `timer.c:age_routes()`: (S,G) `add_jp_entry()` flag fixed, SPT
  switch related.
- `kern.c:k_get_sg_cnt()`: modified to compensate for the kernel's
  return code bug for getting (S,G) byte count (`SIOCGETSGCNT`)
- `pim_proto.c:receive_pim_register()`: if the (S,G) oif is NULL, now
  checks whether the iif is `register_vif`


v2.1.0-alpha9 - 1998-02-18
--------------------------

### Changes
- "non-commersial" statement deleted from the copyright message
- mrinfo support added
- mtrace support added (not completed and not enough tested)
- if invalid local address for `cand_rp` or `cand_bootstrap_router` in
  `pimd.conf`, automatically will use the largest local multicast
  enabled address
- include directory for FreeBSD and SunOS added, so now pimd can be
  compiled without having the necesary include files added to your
  system. Probably a bad idea and may remove it later.
- Fixed some default values for the IP header of IGMP and PIM packets
- `VIFF_PIM_NBR` and `VIFF_DVMRP_NBR` flags added
- `VIFF_REGISTER` now included in the RSRR vifs report
- `find_route()` debug messages removed
- #ifdef for `HAVE_SA_LEN` corrected
- `debug.c`: small fixes


v2.1.0-alpha8 - 1997-11-23
--------------------------

### Fixes
- BSDI related bug fix in defs.h
- small changes in Makefile


v2.1.0-alpha7 - 1997-11-23
--------------------------

### Changes
- RSRR support for `(*,G)` completed
- BSDI 3.0/3.1 support by Hitoshi Asaeda <asaeda@yamato.ibm.co.jp>
  (the kernel patches will be available soon)
- Improved debug messages format, thanks to Hitoshi Asaeda
- A new function `netname()` for network IP address print instead of
  `inet_fmts()`, thanks to Hitoshi Asaeda.
- `pimd.conf`: format changed


v2.1.0-alpha6 - 1997-11-20
--------------------------

### Fixes
- Remove inherited leaves from (S,G) when a receiver drops membership
- some parameters when calling `change_interface()` fixed
- Use `send_pim_null_register` + take the appropriate action when the
  register suppression timer expires
- bug fix related to choosing the largest local IP address for little
  endian machines.


v2.1.0-alpha5
-------------

### Fixes
- `main.c:main()`: startup message fix
- `timer.c:age_routes()`: bug fix in debug code


v2.1.0-alpha4 - 1997-10-31
--------------------------

### Changes
- Minor changes, so pimd now compiles for SunOS 4.1.3 (cc, gcc)

### Fixes
- `pim_proto.csend_periodic_pim_join_prune()`: bug fix thanks to SunOS
  cc warning(!), only affects the `(*,*,RP)` stuff.
- `pimd.conf`: two errors, related to the rate limit fixed


v2.1.0-alpha3 - 1997-10-13
--------------------------

### Changes
- `Makefile`: cleanup
- `defs.h`: cleanup
- `routesock.c`: cleanup

### Fixes
- `igmp_proto.c:accept_group_report()`: bug fixes
- `pim_proto.c:receive_pim_hello()`: bug fixes
  `route.c:change_interfaces()`: bug fixes
- `rp.c`: bug fixes in `init_rp_and_bsr()`, `add_cand_rp()`, and
  `create_pim_bootstrap_message()`


v2.1.0-alpha2 - 1997-09-23
--------------------------

### Changes
- `Makefile`: "make diff" code added
- `debug.c`: debug output slightly changed

### Fixes
- `defs.h:*TIMEOUT()`: definitions fixed
- `route.c`: bugs fixed in `change_interface()` and
  `switch_shortest_path()`
- `timer.c:age_routes()`: number of bugs fixed


v2.1.0-alpha1 - 1997-08-26
--------------------------

### Changes

First alpha version of the "new, up to date" pimd.  RSRR and Solaris
support added.  Many functions rewritten and/or modified.

[UNRELEASED]: https://github.com/troglobit/pimd/compare/2.3.2...HEAD
[v3.0.0]:     https://github.com/troglobit/pimd/compare/2.3.2...3.0.0
[v2.3.2]:     https://github.com/troglobit/pimd/compare/2.3.1...2.3.2
[v2.3.1]:     https://github.com/troglobit/pimd/compare/2.3.0...2.3.1
[v2.3.0]:     https://github.com/troglobit/pimd/compare/2.2.1...2.3.0
[v2.2.1]:     https://github.com/troglobit/pimd/compare/2.2.0...2.2.1
[v2.2.0]:     https://github.com/troglobit/pimd/compare/2.1.8...2.2.0
[v2.1.8]:     https://github.com/troglobit/pimd/compare/2.1.7...2.1.8
[v2.1.7]:     https://github.com/troglobit/pimd/compare/2.1.6...2.1.7
[v2.1.6]:     https://github.com/troglobit/pimd/compare/2.1.5...2.1.6
[v2.1.5]:     https://github.com/troglobit/pimd/compare/2.1.4...2.1.5
[v2.1.4]:     https://github.com/troglobit/pimd/compare/2.1.3...2.1.4
[v2.1.3]:     https://github.com/troglobit/pimd/compare/2.1.2...2.1.3
[v2.1.2]:     https://github.com/troglobit/pimd/compare/2.1.1...2.1.2
[v2.1.1]:     https://github.com/troglobit/pimd/compare/2.1.0...2.1.1
[v2.1.0]:     https://github.com/troglobit/pimd/compare/BASE...2.1.0

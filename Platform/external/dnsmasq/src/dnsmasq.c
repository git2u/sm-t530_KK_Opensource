/* dnsmasq is Copyright (c) 2000-2012 Simon Kelley

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 dated June, 1991, or
   (at your option) version 3 dated 29 June, 2007.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
     
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* Declare static char *compiler_opts  in config.h */
#define DNSMASQ_COMPILE_OPTS

#include "dnsmasq.h"

struct daemon *daemon;

static volatile pid_t pid = 0;
static volatile int pipewrite;

static int set_dns_listeners(time_t now, fd_set *set, int *maxfdp);
static void check_dns_listeners(fd_set *set, time_t now);
static void sig_handler(int sig);
static void async_event(int pipe, time_t now);
static void fatal_event(struct event_desc *ev, char *msg);
static int read_event(int fd, struct event_desc *evp, char **msg);
#ifdef __ANDROID__
static int set_android_listeners(fd_set *set, int *maxfdp);
static int check_android_listeners(fd_set *set);
#endif

int main (int argc, char **argv)
{
  int bind_fallback = 0;
  time_t now;
  struct sigaction sigact;
  struct iname *if_tmp;
  int piperead, pipefd[2], err_pipe[2];
  struct passwd *ent_pw = NULL;
#if defined(HAVE_SCRIPT)
  uid_t script_uid = 0;
  gid_t script_gid = 0;
#endif
  struct group *gp = NULL;
  long i, max_fd = sysconf(_SC_OPEN_MAX);
  char *baduser = NULL;
  int log_err;
#if defined(HAVE_LINUX_NETWORK)
  cap_user_header_t hdr = NULL;
  cap_user_data_t data = NULL;
#endif 

#ifdef LOCALEDIR
  setlocale(LC_ALL, "");
  bindtextdomain("dnsmasq", LOCALEDIR); 
  textdomain("dnsmasq");
#endif

  sigact.sa_handler = sig_handler;
  sigact.sa_flags = 0;
  sigemptyset(&sigact.sa_mask);
  sigaction(SIGUSR1, &sigact, NULL);
  sigaction(SIGUSR2, &sigact, NULL);
  sigaction(SIGHUP, &sigact, NULL);
  sigaction(SIGTERM, &sigact, NULL);
  sigaction(SIGALRM, &sigact, NULL);
  sigaction(SIGCHLD, &sigact, NULL);

  /* ignore SIGPIPE */
  sigact.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &sigact, NULL);

  umask(022); /* known umask, create leases and pid files as 0644 */

  my_syslog(MS_DHCP | LOG_INFO, _("dnsmasq main start"));

  read_opts(argc, argv, compile_opts);
    
  if (daemon->edns_pktsz < PACKETSZ)
    daemon->edns_pktsz = PACKETSZ;
  daemon->packet_buff_sz = daemon->edns_pktsz > DNSMASQ_PACKETSZ ? 
    daemon->edns_pktsz : DNSMASQ_PACKETSZ;
  daemon->packet = safe_malloc(daemon->packet_buff_sz);

  daemon->addrbuff = safe_malloc(ADDRSTRLEN);

#ifdef HAVE_DHCP
  if (!daemon->lease_file)
    {
      if (daemon->dhcp || daemon->dhcp6)
	daemon->lease_file = LEASEFILE;
    }
#endif
  
  /* Close any file descriptors we inherited apart from std{in|out|err} 
     
     Ensure that at least stdin, stdout and stderr (fd 0, 1, 2) exist,
     otherwise file descriptors we create can end up being 0, 1, or 2 
     and then get accidentally closed later when we make 0, 1, and 2 
     open to /dev/null. Normally we'll be started with 0, 1 and 2 open, 
     but it's not guaranteed. By opening /dev/null three times, we 
     ensure that we're not using those fds for real stuff. */
  for (i = 0; i < max_fd; i++)
    if (i != STDOUT_FILENO && i != STDERR_FILENO && i != STDIN_FILENO)
      close(i);
    else
      open("/dev/null", O_RDWR); 


#ifndef HAVE_LINUX_NETWORK
#  if !(defined(IP_RECVDSTADDR) && defined(IP_RECVIF) && defined(IP_SENDSRCADDR))
  if (!option_bool(OPT_NOWILD))
    {
      bind_fallback = 1;
      set_option_bool(OPT_NOWILD);
    }
#  endif
#endif

#ifndef HAVE_TFTP
  if (daemon->tftp_unlimited || daemon->tftp_interfaces)
    die(_("TFTP server not available: set HAVE_TFTP in src/config.h"), NULL, EC_BADCONF);
#endif

#ifdef HAVE_CONNTRACK
  if (option_bool(OPT_CONNTRACK) && (daemon->query_port != 0 || daemon->osport))
    die (_("Cannot use --conntrack AND --query-port"), NULL, EC_BADCONF); 
#else
  if (option_bool(OPT_CONNTRACK))
    die(_("Conntrack support not available: set HAVE_CONNTRACK in src/config.h"), NULL, EC_BADCONF);
#endif

#ifdef HAVE_SOLARIS_NETWORK
  if (daemon->max_logs != 0)
    die(_("asychronous logging is not available under Solaris"), NULL, EC_BADCONF);
#endif
  
#ifdef __ANDROID__
  if (daemon->max_logs != 0)
    die(_("asychronous logging is not available under Android"), NULL, EC_BADCONF);
#endif

  rand_init();
  
  now = dnsmasq_time();
  
#ifdef HAVE_DHCP
  if (daemon->dhcp || daemon->dhcp6)
    {
      /* Note that order matters here, we must call lease_init before
	 creating any file descriptors which shouldn't be leaked
	 to the lease-script init process. We need to call common_init
	 before lease_init to allocate buffers it uses.*/
      dhcp_common_init();
      lease_init(now);
  
      if (daemon->dhcp)
        dhcp_init();
    }
  
#ifdef HAVE_DHCP6
  /* Start RA subsystem if --enable-ra OR dhcp-range=<subnet>, ra-only */
  if (daemon->ra_contexts || option_bool(OPT_RA))
  {
    /* link the DHCP6 contexts to the ra-only ones so we can traverse them all 
    from ->ra_contexts, but only the non-ra-onlies from ->dhcp6 */
    struct dhcp_context *context;

    if (!daemon->ra_contexts)
      daemon->ra_contexts = daemon->dhcp6;
    else
    {
      for (context = daemon->ra_contexts; context->next; context = context->next);
      context->next = daemon->dhcp6;
    }
    ra_init(now);
  }

  if(daemon->dhcp6)
    dhcp6_init();
  else
    my_syslog(MS_DHCP | LOG_INFO, _("daemon->dhcp6 value is NULL"));

#endif

#endif

#ifdef HAVE_LINUX_NETWORK
  /* After lease_init */
  netlink_init();
#endif

#ifdef HAVE_DHCP6
  /* after netlink_init */
  if (daemon->ra_contexts || daemon->dhcp6)
    join_multicast();
#endif

#ifdef HAVE_DHCP
  /* after netlink_init */
  if (daemon->dhcp || daemon->dhcp6)
    lease_find_interfaces(now);
#endif

  if (!enumerate_interfaces())
    die(_("failed to find list of interfaces: %s"), NULL, EC_MISC);
  
  if (option_bool(OPT_NOWILD)) 
    {
      create_bound_listeners(1);

      for (if_tmp = daemon->if_names; if_tmp; if_tmp = if_tmp->next)
	if (if_tmp->name && !if_tmp->used)
	  die(_("unknown interface %s"), if_tmp->name, EC_BADNET);

#if defined(HAVE_LINUX_NETWORK) && defined(HAVE_DHCP)
      /* after enumerate_interfaces()  */
      if (daemon->dhcp)
	{
	  bindtodevice(daemon->dhcpfd);
	  if (daemon->enable_pxe)
	    bindtodevice(daemon->pxefd);
	}
#endif

#if defined(HAVE_LINUX_NETWORK) && defined(HAVE_DHCP6)
      if (daemon->dhcp6)
	bindtodevice(daemon->dhcp6fd);
#endif
    }
  else 
    create_wildcard_listeners();
  
  if (daemon->port != 0)
    cache_init();
    
  if (option_bool(OPT_DBUS))
#ifdef HAVE_DBUS
    {
      char *err;
      daemon->dbus = NULL;
      daemon->watches = NULL;
      if ((err = dbus_init()))
	die(_("DBus error: %s"), err, EC_MISC);
    }
#else
  die(_("DBus not available: set HAVE_DBUS in src/config.h"), NULL, EC_BADCONF);
#endif
  
  if (daemon->port != 0)
    pre_allocate_sfds();

#if defined(HAVE_SCRIPT)
  /* Note getpwnam returns static storage */
  if ((daemon->dhcp || daemon->dhcp6) && 
      daemon->scriptuser && 
      (daemon->lease_change_command || daemon->luascript))
    {
      if ((ent_pw = getpwnam(daemon->scriptuser)))
	{
	  script_uid = ent_pw->pw_uid;
	  script_gid = ent_pw->pw_gid;
	 }
      else
	baduser = daemon->scriptuser;
    }
#endif
  
  if (daemon->username && !(ent_pw = getpwnam(daemon->username)))
    baduser = daemon->username;
  else if (daemon->groupname && !(gp = getgrnam(daemon->groupname)))
    baduser = daemon->groupname;

  if (baduser)
    die(_("unknown user or group: %s"), baduser, EC_BADCONF);
   
  /* implement group defaults, "dip" if available, or group associated with uid */
  if (!daemon->group_set && !gp)
    {
      if (!(gp = getgrnam(CHGRP)) && ent_pw)
	gp = getgrgid(ent_pw->pw_gid);
      
      /* for error message */
      if (gp)
	daemon->groupname = gp->gr_name; 
    }

#if defined(HAVE_LINUX_NETWORK)
  /* determine capability API version here, while we can still
     call safe_malloc */
  if (ent_pw && ent_pw->pw_uid != 0)
    {
      int capsize = 1; /* for header version 1 */
      hdr = safe_malloc(sizeof(*hdr));

      /* find version supported by kernel */
      memset(hdr, 0, sizeof(*hdr));
      capget(hdr, NULL);
      
      if (hdr->version != LINUX_CAPABILITY_VERSION_1)
	{
	  /* if unknown version, use largest supported version (3) */
	  if (hdr->version != LINUX_CAPABILITY_VERSION_2)
	    hdr->version = LINUX_CAPABILITY_VERSION_3;
	  capsize = 2;
	}
      
      data = safe_malloc(sizeof(*data) * capsize);
      memset(data, 0, sizeof(*data) * capsize);
    }
#endif

  /* Use a pipe to carry signals and other events back to the event loop 
     in a race-free manner and another to carry errors to daemon-invoking process */
  safe_pipe(pipefd, 1);
  
  piperead = pipefd[0];
  pipewrite = pipefd[1];
  /* prime the pipe to load stuff first time. */
  send_event(pipewrite, EVENT_RELOAD, 0, NULL); 

  err_pipe[1] = -1;
  
  if (!option_bool(OPT_DEBUG))   
    {
      /* The following code "daemonizes" the process. 
	 See Stevens section 12.4 */
      
      if (chdir("/") != 0)
	die(_("cannot chdir to filesystem root: %s"), NULL, EC_MISC); 

#ifndef NO_FORK      
      if (!option_bool(OPT_NO_FORK))
	{
	  pid_t pid;
	  
	  /* pipe to carry errors back to original process.
	     When startup is complete we close this and the process terminates. */
	  safe_pipe(err_pipe, 0);
	  
	  if ((pid = fork()) == -1)
	    /* fd == -1 since we've not forked, never returns. */
	    send_event(-1, EVENT_FORK_ERR, errno, NULL);
	   
	  if (pid != 0)
	    {
	      struct event_desc ev;
	      char *msg;

	      /* close our copy of write-end */
	      close(err_pipe[1]);
	      
	      /* check for errors after the fork */
	      if (read_event(err_pipe[0], &ev, &msg))
		fatal_event(&ev, msg);
	      
	      _exit(EC_GOOD);
	    } 
	  
	  close(err_pipe[0]);

	  /* NO calls to die() from here on. */
	  
	  setsid();
	 
	  if ((pid = fork()) == -1)
	    send_event(err_pipe[1], EVENT_FORK_ERR, errno, NULL);
	 
	  if (pid != 0)
	    _exit(0);
	}
#endif
            
      /* write pidfile _after_ forking ! */
      if (daemon->runfile)
	{
	  FILE *pidfile;
	  
	  /* only complain if started as root */
	  if ((pidfile = fopen(daemon->runfile, "w")))
	    {
	      fprintf(pidfile, "%d\n", (int) getpid());
	      fclose(pidfile);
	    }
	  else if (getuid() == 0)
	    {
	      send_event(err_pipe[1], EVENT_PIDFILE, errno, daemon->runfile);
	      _exit(0);
	    }
	}
    }
  
   log_err = log_start(ent_pw, err_pipe[1]);

   if (!option_bool(OPT_DEBUG)) 
     {
#ifndef __ANDROID__	        
       /* open  stdout etc to /dev/null */
       int nullfd = open("/dev/null", O_RDWR);
       dup2(nullfd, STDOUT_FILENO);
       dup2(nullfd, STDERR_FILENO);
       dup2(nullfd, STDIN_FILENO);
       close(nullfd);
#endif
     }
   
   /* if we are to run scripts, we need to fork a helper before dropping root. */
  daemon->helperfd = -1;
#ifdef HAVE_SCRIPT 
  if ((daemon->dhcp || daemon->dhcp6) && (daemon->lease_change_command || daemon->luascript))
    daemon->helperfd = create_helper(pipewrite, err_pipe[1], script_uid, script_gid, max_fd);
#endif

  if (!option_bool(OPT_DEBUG) && getuid() == 0)   
    {
      int bad_capabilities = 0;
      gid_t dummy;
      
      /* remove all supplimentary groups */
      if (gp && 
	  (setgroups(0, &dummy) == -1 ||
	   setgid(gp->gr_gid) == -1))
	{
	  send_event(err_pipe[1], EVENT_GROUP_ERR, errno, daemon->groupname);
	  _exit(0);
	}
  
      if (ent_pw && ent_pw->pw_uid != 0)
	{     
#if defined(HAVE_LINUX_NETWORK)	  
	  /* On linux, we keep CAP_NETADMIN (for ARP-injection) and
	     CAP_NET_RAW (for icmp) if we're doing dhcp. If we have yet to bind 
	     ports because of DAD, we need CAP_NET_BIND_SERVICE too. */
	  if (is_dad_listeners())
	    data->effective = data->permitted = data->inheritable =
	      (1 << CAP_NET_ADMIN) | (1 << CAP_NET_RAW) | 
	      (1 << CAP_SETUID) | (1 << CAP_NET_BIND_SERVICE);
	  else
	    data->effective = data->permitted = data->inheritable =
	      (1 << CAP_NET_ADMIN) | (1 << CAP_NET_RAW) | (1 << CAP_SETUID);
	  
	  /* Tell kernel to not clear capabilities when dropping root */
	  if (capset(hdr, data) == -1 || prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0) == -1)
	    bad_capabilities = errno;
			  
#elif defined(HAVE_SOLARIS_NETWORK)
	  /* http://developers.sun.com/solaris/articles/program_privileges.html */
	  priv_set_t *priv_set;
	  
	  if (!(priv_set = priv_str_to_set("basic", ",", NULL)) ||
	      priv_addset(priv_set, PRIV_NET_ICMPACCESS) == -1 ||
	      priv_addset(priv_set, PRIV_SYS_NET_CONFIG) == -1)
	    bad_capabilities = errno;

	  if (priv_set && bad_capabilities == 0)
	    {
	      priv_inverse(priv_set);
	  
	      if (setppriv(PRIV_OFF, PRIV_LIMIT, priv_set) == -1)
		bad_capabilities = errno;
	    }

	  if (priv_set)
	    priv_freeset(priv_set);

#endif    

	  if (bad_capabilities != 0)
	    {
	      send_event(err_pipe[1], EVENT_CAP_ERR, bad_capabilities, NULL);
	      _exit(0);
	    }
	  
	  /* finally drop root */
	  if (setuid(ent_pw->pw_uid) == -1)
	    {
	      send_event(err_pipe[1], EVENT_USER_ERR, errno, daemon->username);
	      _exit(0);
	    }     

#ifdef HAVE_LINUX_NETWORK
	 if (is_dad_listeners())
	   data->effective = data->permitted =
	     (1 << CAP_NET_ADMIN) | (1 << CAP_NET_RAW) | (1 << CAP_NET_BIND_SERVICE);
	 else
	   data->effective = data->permitted = 
	     (1 << CAP_NET_ADMIN) | (1 << CAP_NET_RAW);
	  data->inheritable = 0;
	  
	  /* lose the setuid and setgid capbilities */
	  if (capset(hdr, data) == -1)
	    {
	      send_event(err_pipe[1], EVENT_CAP_ERR, errno, NULL);
	      _exit(0);
	    }
#endif
	  
	}
    }
  
#ifdef HAVE_LINUX_NETWORK
  if (option_bool(OPT_DEBUG)) 
    prctl(PR_SET_DUMPABLE, 1, 0, 0, 0);
#endif

  if (daemon->port == 0)
    my_syslog(LOG_INFO, _("started, version %s DNS disabled"), VERSION);
  else if (daemon->cachesize != 0)
    my_syslog(LOG_INFO, _("started, version %s cachesize %d"), VERSION, daemon->cachesize);
  else
    my_syslog(LOG_INFO, _("started, version %s cache disabled"), VERSION);
  
  my_syslog(LOG_INFO, _("compile time options: %s"), compile_opts);
  
#ifdef HAVE_DBUS
  if (option_bool(OPT_DBUS))
    {
      if (daemon->dbus)
	my_syslog(LOG_INFO, _("DBus support enabled: connected to system bus"));
      else
	my_syslog(LOG_INFO, _("DBus support enabled: bus connection pending"));
    }
#endif

  if (log_err != 0)
    my_syslog(LOG_WARNING, _("warning: failed to change owner of %s: %s"), 
	      daemon->log_file, strerror(log_err));

  if (bind_fallback)
    my_syslog(LOG_WARNING, _("setting --bind-interfaces option because of OS limitations"));
  
  if (!option_bool(OPT_NOWILD)) 
    for (if_tmp = daemon->if_names; if_tmp; if_tmp = if_tmp->next)
      if (if_tmp->name && !if_tmp->used)
	my_syslog(LOG_WARNING, _("warning: interface %s does not currently exist"), if_tmp->name);
   
  if (daemon->port != 0 && option_bool(OPT_NO_RESOLV))
    {
      if (daemon->resolv_files && !daemon->resolv_files->is_default)
	my_syslog(LOG_WARNING, _("warning: ignoring resolv-file flag because no-resolv is set"));
      daemon->resolv_files = NULL;
      if (!daemon->servers)
	my_syslog(LOG_WARNING, _("warning: no upstream servers configured"));
    } 

  if (daemon->max_logs != 0)
    my_syslog(LOG_INFO, _("asynchronous logging enabled, queue limit is %d messages"), daemon->max_logs);
 
  if (daemon->ra_contexts)
    my_syslog(MS_DHCP | LOG_INFO, _("IPv6 router advertisement enabled"));

#ifdef HAVE_DHCP
  if (daemon->dhcp || daemon->dhcp6 || daemon->ra_contexts)
    {
      struct dhcp_context *dhcp_tmp;
      int family = AF_INET;
      dhcp_tmp = daemon->dhcp;
     
#ifdef HAVE_DHCP6
    again:
#endif
      for (; dhcp_tmp; dhcp_tmp = dhcp_tmp->next)
	{
	  void *start = &dhcp_tmp->start;
	  void *end = &dhcp_tmp->end;
	  	  
#ifdef HAVE_DHCP6
	  if (family == AF_INET6)
	    {
	      start = &dhcp_tmp->start6;
	      end = &dhcp_tmp->end6;
	      struct in6_addr subnet = dhcp_tmp->start6;
	      setaddr6part(&subnet, 0);
	      inet_ntop(AF_INET6, &subnet, daemon->addrbuff, 256);
	    }
#endif
	  
	  if (family != AF_INET && (dhcp_tmp->flags & CONTEXT_DEPRECATE))
	    strcpy(daemon->namebuff, _("prefix deprecated"));
	  else
	    {
	      char *p = daemon->namebuff;
	      p += sprintf(p, _("lease time "));
	      prettyprint_time(p, dhcp_tmp->lease_time);
	    }
	  
	  if (daemon->dhcp_buff)
	    inet_ntop(family, start, daemon->dhcp_buff, 256);
	  if (daemon->dhcp_buff3)
	    inet_ntop(family, end, daemon->dhcp_buff3, 256);
	  if ((dhcp_tmp->flags & CONTEXT_DHCP) || family == AF_INET) 
	    my_syslog(MS_DHCP | LOG_INFO, 
		      (dhcp_tmp->flags & CONTEXT_RA_STATELESS) ? 
		      _("stateless DHCPv6 on %s%.0s%.0s") :
		      (dhcp_tmp->flags & CONTEXT_STATIC) ? 
		      _("DHCP, static leases only on %.0s%s, %s") :
		      (dhcp_tmp->flags & CONTEXT_PROXY) ?
		      _("DHCP, proxy on subnet %.0s%s%.0s") :
		      _("DHCP, IP range %s -- %s, %s"),
		      daemon->dhcp_buff, daemon->dhcp_buff3, daemon->namebuff);

	  if (dhcp_tmp->flags & CONTEXT_RA_NAME)
	    my_syslog(MS_DHCP | LOG_INFO, _("DHCPv4-derived IPv6 names on %s"), 
		      daemon->addrbuff);
	  if (dhcp_tmp->flags & (CONTEXT_RA_ONLY | CONTEXT_RA_NAME | CONTEXT_RA_STATELESS))
    {
      if (!(dhcp_tmp->flags & CONTEXT_DEPRECATE))
      {
        char *p = daemon->namebuff;
        p += sprintf(p, _("prefix valid "));
        prettyprint_time(p, dhcp_tmp->lease_time > 7200 ? dhcp_tmp->lease_time : 7200);
      }
      my_syslog(MS_DHCP | LOG_INFO, _("SLAAC on %s %s"), 
      daemon->addrbuff, daemon->namebuff);
    }
	}
      
#ifdef HAVE_DHCP6
      if (family == AF_INET)
	{
	  family = AF_INET6;
	  if (daemon->ra_contexts)
	    dhcp_tmp = daemon->ra_contexts;
	  else
	    dhcp_tmp = daemon->dhcp6;
	  goto again;
	}
#endif

    }
#endif


#ifdef HAVE_TFTP
  if (daemon->tftp_unlimited || daemon->tftp_interfaces)
    {
#ifdef FD_SETSIZE
      if (FD_SETSIZE < (unsigned)max_fd)
	max_fd = FD_SETSIZE;
#endif

      my_syslog(MS_TFTP | LOG_INFO, "TFTP %s%s %s", 
		daemon->tftp_prefix ? _("root is ") : _("enabled"),
		daemon->tftp_prefix ? daemon->tftp_prefix: "",
		option_bool(OPT_TFTP_SECURE) ? _("secure mode") : "");
      
      /* This is a guess, it assumes that for small limits, 
	 disjoint files might be served, but for large limits, 
	 a single file will be sent to may clients (the file only needs
	 one fd). */

      max_fd -= 30; /* use other than TFTP */
      
      if (max_fd < 0)
	max_fd = 5;
      else if (max_fd < 100)
	max_fd = max_fd/2;
      else
	max_fd = max_fd - 20;
      
      /* if we have to use a limited range of ports, 
	 that will limit the number of transfers */
      if (daemon->start_tftp_port != 0 &&
	  daemon->end_tftp_port - daemon->start_tftp_port + 1 < max_fd)
	max_fd = daemon->end_tftp_port - daemon->start_tftp_port + 1;

      if (daemon->tftp_max > max_fd)
	{
	  daemon->tftp_max = max_fd;
	  my_syslog(MS_TFTP | LOG_WARNING, 
		    _("restricting maximum simultaneous TFTP transfers to %d"), 
		    daemon->tftp_max);
	}
    }
#endif

  /* finished start-up - release original process */
  if (err_pipe[1] != -1)
    close(err_pipe[1]);
  
  if (daemon->port != 0)
    check_servers();
  
  pid = getpid();
  
  while (1)
    {
      int maxfd = -1;
      struct timeval t, *tp = NULL;
      fd_set rset, wset, eset;
      
      FD_ZERO(&rset);
      FD_ZERO(&wset);
      FD_ZERO(&eset);
      
      /* if we are out of resources, find how long we have to wait
	 for some to come free, we'll loop around then and restart
	 listening for queries */
      if ((t.tv_sec = set_dns_listeners(now, &rset, &maxfd)) != 0)
	{
	  t.tv_usec = 0;
	  tp = &t;
	}
#ifdef __ANDROID__
      set_android_listeners(&rset, &maxfd);
#endif

      /* Whilst polling for the dbus, or doing a tftp transfer, wake every quarter second */
      if (daemon->tftp_trans ||
	  (option_bool(OPT_DBUS) && !daemon->dbus))
	{
	  t.tv_sec = 0;
	  t.tv_usec = 250000;
	  tp = &t;
	}
      /* Wake every second whilst waiting for DAD to complete */
      else if (is_dad_listeners())
	{
	  t.tv_sec = 1;
	  t.tv_usec = 0;
	  tp = &t;
	}

#ifdef HAVE_DBUS
      set_dbus_listeners(&maxfd, &rset, &wset, &eset);
#endif	
  
#ifdef HAVE_DHCP
      if (daemon->dhcp)
	{
	  FD_SET(daemon->dhcpfd, &rset);
	  bump_maxfd(daemon->dhcpfd, &maxfd);
	  if (daemon->pxefd != -1)
	    {
	      FD_SET(daemon->pxefd, &rset);
	      bump_maxfd(daemon->pxefd, &maxfd);
	    }
	}
#endif

#ifdef HAVE_DHCP6
      if (daemon->dhcp6)
	{
	  FD_SET(daemon->dhcp6fd, &rset);
	  bump_maxfd(daemon->dhcp6fd, &maxfd);
	}

      if (daemon->ra_contexts)
	{
	  FD_SET(daemon->icmp6fd, &rset);
	  bump_maxfd(daemon->icmp6fd, &maxfd); 
	}
#endif

#ifdef HAVE_LINUX_NETWORK
      FD_SET(daemon->netlinkfd, &rset);
      bump_maxfd(daemon->netlinkfd, &maxfd);
#endif
      
      FD_SET(piperead, &rset);
      bump_maxfd(piperead, &maxfd);

#ifdef HAVE_DHCP
#  ifdef HAVE_SCRIPT
      while (helper_buf_empty() && do_script_run(now));

#    ifdef HAVE_TFTP
      while (helper_buf_empty() && do_tftp_script_run());
#    endif

      if (!helper_buf_empty())
	{
	  FD_SET(daemon->helperfd, &wset);
	  bump_maxfd(daemon->helperfd, &maxfd);
	}
#  else
      /* need this for other side-effects */
      while (do_script_run(now));

#    ifdef HAVE_TFTP 
      while (do_tftp_script_run());
#    endif

#  endif
#endif
   
      /* must do this just before select(), when we know no
	 more calls to my_syslog() can occur */
      set_log_writer(&wset, &maxfd);
      
      if (select(maxfd+1, &rset, &wset, &eset, tp) < 0)
	{
	  /* otherwise undefined after error */
	  FD_ZERO(&rset); FD_ZERO(&wset); FD_ZERO(&eset);
	}

      now = dnsmasq_time();

      check_log_writer(&wset);
      
      /* Check the interfaces to see if any have exited DAD state
	 and if so, bind the address. */
      if (is_dad_listeners())
	{
	  enumerate_interfaces();
	  /* NB, is_dad_listeners() == 1 --> we're binding interfaces */
	  create_bound_listeners(0);
	}

#ifdef HAVE_LINUX_NETWORK
      if (FD_ISSET(daemon->netlinkfd, &rset))
	netlink_multicast();
#endif

      /* Check for changes to resolv files once per second max. */
      /* Don't go silent for long periods if the clock goes backwards. */
      if (daemon->last_resolv == 0 || 
	  difftime(now, daemon->last_resolv) > 1.0 || 
	  difftime(now, daemon->last_resolv) < -1.0)
	{
	  /* poll_resolv doesn't need to reload first time through, since 
	     that's queued anyway. */

	  poll_resolv(0, daemon->last_resolv != 0, now); 	  
	  daemon->last_resolv = now;
	}
      
      if (FD_ISSET(piperead, &rset))
	async_event(piperead, now);
      
#ifdef HAVE_DBUS
      /* if we didn't create a DBus connection, retry now. */ 
     if (option_bool(OPT_DBUS) && !daemon->dbus)
	{
	  char *err;
	  if ((err = dbus_init()))
	    my_syslog(LOG_WARNING, _("DBus error: %s"), err);
	  if (daemon->dbus)
	    my_syslog(LOG_INFO, _("connected to system DBus"));
	}
      check_dbus_listeners(&rset, &wset, &eset);
#endif

#ifdef __ANDROID__
      check_android_listeners(&rset);
#endif
      
      check_dns_listeners(&rset, now);

#ifdef HAVE_TFTP
      check_tftp_listeners(&rset, now);
#endif      

#ifdef HAVE_DHCP
      if (daemon->dhcp)
	{
	  if (FD_ISSET(daemon->dhcpfd, &rset))
	    dhcp_packet(now, 0);
	  if (daemon->pxefd != -1 && FD_ISSET(daemon->pxefd, &rset))
	    dhcp_packet(now, 1);
	}
 
#ifdef HAVE_DHCP6
      my_syslog(MS_DHCP | LOG_INFO, _("Check FD_ISSET(daemon->dhcp6fd, &rset)"));
      if (daemon->dhcp6 && FD_ISSET(daemon->dhcp6fd, &rset))
      {
        my_syslog(MS_DHCP | LOG_INFO, _("dhcp6_packet"));
	      dhcp6_packet(now);
      }

      if (daemon->ra_contexts && FD_ISSET(daemon->icmp6fd, &rset))
	    icmp6_packet();
#endif

#  ifdef HAVE_SCRIPT
      if (daemon->helperfd != -1 && FD_ISSET(daemon->helperfd, &wset))
	helper_write();
#  endif
#endif

    }
}

static void sig_handler(int sig)
{
  if (pid == 0)
    {
      /* ignore anything other than TERM during startup
	 and in helper proc. (helper ignore TERM too) */
      if (sig == SIGTERM)
	exit(EC_MISC);
    }
  else if (pid != getpid())
    {
      /* alarm is used to kill TCP children after a fixed time. */
      if (sig == SIGALRM)
	_exit(0);
    }
  else
    {
      /* master process */
      int event, errsave = errno;
      
      if (sig == SIGHUP)
	event = EVENT_RELOAD;
      else if (sig == SIGCHLD)
	event = EVENT_CHILD;
      else if (sig == SIGALRM)
	event = EVENT_ALARM;
      else if (sig == SIGTERM)
	event = EVENT_TERM;
      else if (sig == SIGUSR1)
	event = EVENT_DUMP;
      else if (sig == SIGUSR2)
	event = EVENT_REOPEN;
      else
	return;

      send_event(pipewrite, event, 0, NULL); 
      errno = errsave;
    }
}

/* now == 0 -> queue immediate callback */
void send_alarm(time_t event, time_t now)
{
  if (now == 0 || event != 0)
    {
      /* alarm(0) or alarm(-ve) doesn't do what we want.... */
      if ((now == 0 || difftime(event, now) <= 0.0))
	send_event(pipewrite, EVENT_ALARM, 0, NULL);
      else 
	alarm((unsigned)difftime(event, now)); 
    }
}

void send_event(int fd, int event, int data, char *msg)
{
  struct event_desc ev;
  struct iovec iov[2];

  ev.event = event;
  ev.data = data;
  ev.msg_sz = msg ? strlen(msg) : 0;
  
  iov[0].iov_base = &ev;
  iov[0].iov_len = sizeof(ev);
  iov[1].iov_base = msg;
  iov[1].iov_len = ev.msg_sz;
  
  /* error pipe, debug mode. */
  if (fd == -1)
    fatal_event(&ev, msg);
  else
    /* pipe is non-blocking and struct event_desc is smaller than
       PIPE_BUF, so this either fails or writes everything */
    while (writev(fd, iov, msg ? 2 : 1) == -1 && errno == EINTR);
}

/* NOTE: the memory used to return msg is leaked: use msgs in events only
   to describe fatal errors. */
static int read_event(int fd, struct event_desc *evp, char **msg)
{
  char *buf;

  if (!read_write(fd, (unsigned char *)evp, sizeof(struct event_desc), 1))
    return 0;
  
  *msg = NULL;
  
  if (evp->msg_sz != 0 && 
      (buf = malloc(evp->msg_sz + 1)) &&
      read_write(fd, (unsigned char *)buf, evp->msg_sz, 1))
    {
      buf[evp->msg_sz] = 0;
      *msg = buf;
    }

  return 1;
}
    
static void fatal_event(struct event_desc *ev, char *msg)
{
  errno = ev->data;
  
  switch (ev->event)
    {
    case EVENT_DIE:
      exit(0);

    case EVENT_FORK_ERR:
      die(_("cannot fork into background: %s"), NULL, EC_MISC);
  
    case EVENT_PIPE_ERR:
      die(_("failed to create helper: %s"), NULL, EC_MISC);
  
    case EVENT_CAP_ERR:
      die(_("setting capabilities failed: %s"), NULL, EC_MISC);

    case EVENT_USER_ERR:
      die(_("failed to change user-id to %s: %s"), msg, EC_MISC);

    case EVENT_GROUP_ERR:
      die(_("failed to change group-id to %s: %s"), msg, EC_MISC);
      
    case EVENT_PIDFILE:
      die(_("failed to open pidfile %s: %s"), msg, EC_FILE);

    case EVENT_LOG_ERR:
      die(_("cannot open log %s: %s"), msg, EC_FILE);
    
    case EVENT_LUA_ERR:
      die(_("failed to load Lua script: %s"), msg, EC_MISC);
    }
}	
      
static void async_event(int pipe, time_t now)
{
  pid_t p;
  struct event_desc ev;
  int i;
  char *msg;
  
  /* NOTE: the memory used to return msg is leaked: use msgs in events only
     to describe fatal errors. */
  
  if (read_event(pipe, &ev, &msg))
    switch (ev.event)
      {
      case EVENT_RELOAD:
	clear_cache_and_reload(now);
	if (daemon->port != 0 && daemon->resolv_files && option_bool(OPT_NO_POLL))
	  {
	    reload_servers(daemon->resolv_files->name);
	    check_servers();
	  }
#ifdef HAVE_DHCP
	rerun_scripts();
#endif
	break;
	
      case EVENT_DUMP:
	if (daemon->port != 0)
	  dump_cache(now);
	break;
	
      case EVENT_ALARM:
#ifdef HAVE_DHCP
	if (daemon->dhcp || daemon->dhcp6)
	  {
	    lease_prune(NULL, now);
	    lease_update_file(now);
	  }
#ifdef HAVE_DHCP6
	else if (daemon->ra_contexts)
	  /* Not doing DHCP, so no lease system, manage alarms for ra only */
	    send_alarm(periodic_ra(now), now);
#endif
#endif
	break;
		
      case EVENT_CHILD:
	/* See Stevens 5.10 */
	while ((p = waitpid(-1, NULL, WNOHANG)) != 0)
	  if (p == -1)
	    {
	      if (errno != EINTR)
		break;
	    }      
	  else 
	    for (i = 0 ; i < MAX_PROCS; i++)
	      if (daemon->tcp_pids[i] == p)
		daemon->tcp_pids[i] = 0;
	break;
	
      case EVENT_KILLED:
	my_syslog(LOG_WARNING, _("script process killed by signal %d"), ev.data);
	break;

      case EVENT_EXITED:
	my_syslog(LOG_WARNING, _("script process exited with status %d"), ev.data);
	break;

      case EVENT_EXEC_ERR:
	my_syslog(LOG_ERR, _("failed to execute %s: %s"), 
		  daemon->lease_change_command, strerror(ev.data));
	break;

	/* necessary for fatal errors in helper */
      case EVENT_USER_ERR:
      case EVENT_DIE:
      case EVENT_LUA_ERR:
	fatal_event(&ev, msg);
	break;

      case EVENT_REOPEN:
	/* Note: this may leave TCP-handling processes with the old file still open.
	   Since any such process will die in CHILD_LIFETIME or probably much sooner,
	   we leave them logging to the old file. */
	if (daemon->log_file != NULL)
	  log_reopen(daemon->log_file);
	break;
	
      case EVENT_TERM:
	/* Knock all our children on the head. */
	for (i = 0; i < MAX_PROCS; i++)
	  if (daemon->tcp_pids[i] != 0)
	    kill(daemon->tcp_pids[i], SIGALRM);
	
#if defined(HAVE_SCRIPT)
	/* handle pending lease transitions */
	if (daemon->helperfd != -1)
	  {
	    /* block in writes until all done */
	    if ((i = fcntl(daemon->helperfd, F_GETFL)) != -1)
	      fcntl(daemon->helperfd, F_SETFL, i & ~O_NONBLOCK); 
	    do {
	      helper_write();
	    } while (!helper_buf_empty() || do_script_run(now));
	    close(daemon->helperfd);
	  }
#endif
	
	if (daemon->lease_stream)
	  fclose(daemon->lease_stream);

	if (daemon->runfile)
	  unlink(daemon->runfile);
	
	my_syslog(LOG_INFO, _("exiting on receipt of SIGTERM"));
	flush_log();
	exit(EC_GOOD);
      }
}

void poll_resolv(int force, int do_reload, time_t now)
{
  struct resolvc *res, *latest;
  struct stat statbuf;
  time_t last_change = 0;
  /* There may be more than one possible file. 
     Go through and find the one which changed _last_.
     Warn of any which can't be read. */

  if (daemon->port == 0 || option_bool(OPT_NO_POLL))
    return;
  
  for (latest = NULL, res = daemon->resolv_files; res; res = res->next)
    if (stat(res->name, &statbuf) == -1)
      {
	if (force)
	  {
	    res->mtime = 0; 
	    continue;
	  }

	if (!res->logged)
	  my_syslog(LOG_WARNING, _("failed to access %s: %s"), res->name, strerror(errno));
	res->logged = 1;
	
	if (res->mtime != 0)
	  { 
	    /* existing file evaporated, force selection of the latest
	       file even if its mtime hasn't changed since we last looked */
	    poll_resolv(1, do_reload, now);
	    return;
	  }
      }
    else
      {
	res->logged = 0;
	if (force || ((time_t)statbuf.st_mtime != res->mtime))
          {
            res->mtime = statbuf.st_mtime;
	    if (difftime(statbuf.st_mtime, last_change) > 0.0)
	      {
		last_change = statbuf.st_mtime;
		latest = res;
	      }
	  }
      }
  
  if (latest)
    {
      static int warned = 0;
      if (reload_servers(latest->name))
	{
	  my_syslog(LOG_INFO, _("reading %s"), latest->name);
	  warned = 0;
	  check_servers();
	  if (option_bool(OPT_RELOAD) && do_reload)
	    clear_cache_and_reload(now);
	}
      else 
	{
	  latest->mtime = 0;
	  if (!warned)
	    {
	      my_syslog(LOG_WARNING, _("no servers found in %s, will retry"), latest->name);
	      warned = 1;
	    }
	}
    }
}       

void clear_cache_and_reload(time_t now)
{
  if (daemon->port != 0)
    cache_reload();
  
#ifdef HAVE_DHCP
  if (daemon->dhcp || daemon->dhcp6)
    {
      if (option_bool(OPT_ETHERS))
	dhcp_read_ethers();
      reread_dhcp();
      dhcp_update_configs(daemon->dhcp_conf);
      check_dhcp_hosts(0);
      lease_update_from_configs(); 
      lease_update_file(now); 
      lease_update_dns(1);
    }
#ifdef HAVE_DHCP6
  else if (daemon->ra_contexts)
    /* Not doing DHCP, so no lease system, manage 
       alarms for ra only */
    send_alarm(periodic_ra(now), now);
#endif
#endif
}

#ifdef __ANDROID__

static int set_android_listeners(fd_set *set, int *maxfdp) {
    FD_SET(STDIN_FILENO, set);
    bump_maxfd(STDIN_FILENO, maxfdp);
    return 0;
}

static int check_android_listeners(fd_set *set) {
    int retcode = 0;
    if (FD_ISSET(STDIN_FILENO, set)) {
        char buffer[1024]={'\0',};
        int rc;
        int consumed = 0;

        if ((rc = read(STDIN_FILENO, buffer, sizeof(buffer) -1)) < 0) {
            my_syslog(LOG_ERR, _("Error reading from stdin (%s)"), strerror(errno));
            return -1;
        }
        buffer[rc] = '\0';
        while(consumed < rc) {
            char *cmd;
            char *current_cmd = &buffer[consumed];
            char *params = current_cmd;
            int len = strlen(current_cmd);

            cmd = strsep(&params, "|");
            if (!strcmp(cmd, "update_dns")) {
                if (params != NULL) {
                    set_servers(params);
                    check_servers();
                } else {
                    my_syslog(LOG_ERR, _("Malformatted msg '%s'"), current_cmd);
                    retcode = -1;
                }
//<RNTFIX:: Uses samsung's DHCPv6 implementation
            } else if (!strcmp(cmd,"update_v6dns")) {
              if (params != NULL) {
                  my_syslog(MS_DHCP| LOG_INFO, "update_v6dns params:%s", params);
                  set_servers(params);
                  check_servers();
              } else {
                  my_syslog(LOG_ERR, _("Malformatted v6dns msg '%s'"), current_cmd);
                  retcode = -1;
              }
//>RNTFIX
// [ for ipv6 tethering
            } else if (!strcmp(cmd,"update_ra")) {
#ifdef HAVE_DHCP6  
                if(params != NULL){
                    my_syslog(MS_DHCP| LOG_INFO, "update ra:%s", params);
                    set_mode_of_ra(params);
                }else {
                    my_syslog(LOG_ERR, _("Malformatted msg '%s'"), current_cmd);
                    retcode = -1;
                }
#endif
// ] for ipv6 tethering
            } else {
                my_syslog(LOG_ERR, _("Unknown cmd '%s'"), cmd);
                retcode = -1;
            }
            consumed += len + 1;
        }
    }
    return retcode;
}
#endif

static int set_dns_listeners(time_t now, fd_set *set, int *maxfdp)
{
  struct serverfd *serverfdp;
  struct listener *listener;
  int wait = 0, i;
  
#ifdef HAVE_TFTP
  int  tftp = 0;
  struct tftp_transfer *transfer;
  for (transfer = daemon->tftp_trans; transfer; transfer = transfer->next)
    {
      tftp++;
      FD_SET(transfer->sockfd, set);
      bump_maxfd(transfer->sockfd, maxfdp);
    }
#endif
  
  /* will we be able to get memory? */
  if (daemon->port != 0)
    get_new_frec(now, &wait);
  
  for (serverfdp = daemon->sfds; serverfdp; serverfdp = serverfdp->next)
    {
      FD_SET(serverfdp->fd, set);
      bump_maxfd(serverfdp->fd, maxfdp);
    }

  if (daemon->port != 0 && !daemon->osport)
    for (i = 0; i < RANDOM_SOCKS; i++)
      if (daemon->randomsocks[i].refcount != 0)
	{
	  FD_SET(daemon->randomsocks[i].fd, set);
	  bump_maxfd(daemon->randomsocks[i].fd, maxfdp);
	}
  
  for (listener = daemon->listeners; listener; listener = listener->next)
    {
      /* only listen for queries if we have resources */
      if (listener->fd != -1 && wait == 0)
	{
	  FD_SET(listener->fd, set);
	  bump_maxfd(listener->fd, maxfdp);
	}

      /* death of a child goes through the select loop, so
	 we don't need to explicitly arrange to wake up here */
      if  (listener->tcpfd != -1)
	for (i = 0; i < MAX_PROCS; i++)
	  if (daemon->tcp_pids[i] == 0)
	    {
	      FD_SET(listener->tcpfd, set);
	      bump_maxfd(listener->tcpfd, maxfdp);
	      break;
	    }

#ifdef HAVE_TFTP
      if (tftp <= daemon->tftp_max && listener->tftpfd != -1)
	{
	  FD_SET(listener->tftpfd, set);
	  bump_maxfd(listener->tftpfd, maxfdp);
	}
#endif

    }
  
  return wait;
}

static void check_dns_listeners(fd_set *set, time_t now)
{
  struct serverfd *serverfdp;
  struct listener *listener;
  int i;

  for (serverfdp = daemon->sfds; serverfdp; serverfdp = serverfdp->next)
    if (FD_ISSET(serverfdp->fd, set))
      reply_query(serverfdp->fd, serverfdp->source_addr.sa.sa_family, now);
  
  if (daemon->port != 0 && !daemon->osport)
    for (i = 0; i < RANDOM_SOCKS; i++)
      if (daemon->randomsocks[i].refcount != 0 && 
	  FD_ISSET(daemon->randomsocks[i].fd, set))
	reply_query(daemon->randomsocks[i].fd, daemon->randomsocks[i].family, now);
  
  for (listener = daemon->listeners; listener; listener = listener->next)
    {
      if (listener->fd != -1 && FD_ISSET(listener->fd, set))
	receive_query(listener, now); 
      
#ifdef HAVE_TFTP     
      if (listener->tftpfd != -1 && FD_ISSET(listener->tftpfd, set))
	tftp_request(listener, now);
#endif

      if (listener->tcpfd != -1 && FD_ISSET(listener->tcpfd, set))
	{
	  int confd;
	  struct irec *iface = NULL;
	  pid_t p;
	  union mysockaddr tcp_addr;
	  socklen_t tcp_len = sizeof(union mysockaddr);

	  while ((confd = accept(listener->tcpfd, NULL, NULL)) == -1 && errno == EINTR);
	  
	  if (confd == -1 ||
	      getsockname(confd, (struct sockaddr *)&tcp_addr, &tcp_len) == -1)
	    continue;
	  
	  if (option_bool(OPT_NOWILD))
	    iface = listener->iface; /* May be NULL */
	  else
	    {
	      /* Check for allowed interfaces when binding the wildcard address:
		 we do this by looking for an interface with the same address as 
		 the local address of the TCP connection, then looking to see if that's
		 an allowed interface. As a side effect, we get the netmask of the
		 interface too, for localisation. */
	      
	      /* interface may be new since startup */
	      if (enumerate_interfaces())
		for (iface = daemon->interfaces; iface; iface = iface->next)
		  if (sockaddr_isequal(&iface->addr, &tcp_addr))
		    break;
	    }
	  
	  if (!iface && !option_bool(OPT_NOWILD))
	    {
	      shutdown(confd, SHUT_RDWR);
	      close(confd);
	    }
#ifndef NO_FORK
	  else if (!option_bool(OPT_DEBUG) && (p = fork()) != 0)
	    {
	      if (p != -1)
		{
		  int i;
		  for (i = 0; i < MAX_PROCS; i++)
		    if (daemon->tcp_pids[i] == 0)
		      {
			daemon->tcp_pids[i] = p;
			break;
		      }
		}
	      close(confd);
	    }
#endif
	  else
	    {
	      unsigned char *buff;
	      struct server *s; 
	      int flags;
	      struct in_addr netmask;

	      if (iface)
		netmask = iface->netmask;
	      else
		netmask.s_addr = 0;

#ifndef NO_FORK
	      /* Arrange for SIGALARM after CHILD_LIFETIME seconds to
		 terminate the process. */
	      if (!option_bool(OPT_DEBUG))
		alarm(CHILD_LIFETIME);
#endif

	      /* start with no upstream connections. */
	      for (s = daemon->servers; s; s = s->next)
		 s->tcpfd = -1; 
	      
	      /* The connected socket inherits non-blocking
		 attribute from the listening socket. 
		 Reset that here. */
	      if ((flags = fcntl(confd, F_GETFL, 0)) != -1)
		fcntl(confd, F_SETFL, flags & ~O_NONBLOCK);
	      
	      buff = tcp_request(confd, now, &tcp_addr, netmask);
	       
	      shutdown(confd, SHUT_RDWR);
	      close(confd);
	      
	      if (buff)
		free(buff);
	      
	      for (s = daemon->servers; s; s = s->next)
		if (s->tcpfd != -1)
		  {
		    shutdown(s->tcpfd, SHUT_RDWR);
		    close(s->tcpfd);
		  }
#ifndef NO_FORK		   
	      if (!option_bool(OPT_DEBUG))
		{
		  flush_log();
		  _exit(0);
		}
#endif
	    }
	}
    }
}

#ifdef HAVE_DHCP
int make_icmp_sock(void)
{
  int fd;
  int zeroopt = 0;

  if ((fd = socket (AF_INET, SOCK_RAW, IPPROTO_ICMP)) != -1)
    {
      if (!fix_fd(fd) ||
	  setsockopt(fd, SOL_SOCKET, SO_DONTROUTE, &zeroopt, sizeof(zeroopt)) == -1)
	{
	  close(fd);
	  fd = -1;
	}
    }

  return fd;
}

int icmp_ping(struct in_addr addr)
{
  /* Try and get an ICMP echo from a machine. */

  /* Note that whilst in the three second wait, we check for 
     (and service) events on the DNS and TFTP  sockets, (so doing that
     better not use any resources our caller has in use...)
     but we remain deaf to signals or further DHCP packets. */

  int fd;
  struct sockaddr_in saddr;
  struct { 
    struct ip ip;
    struct icmp icmp;
  } packet;
  unsigned short id = rand16();
  unsigned int i, j;
  int gotreply = 0;
  time_t start, now;

#if defined(HAVE_LINUX_NETWORK) || defined (HAVE_SOLARIS_NETWORK)
  if ((fd = make_icmp_sock()) == -1)
    return 0;
#else
  int opt = 2000;
  fd = daemon->dhcp_icmp_fd;
  setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &opt, sizeof(opt));
#endif

  saddr.sin_family = AF_INET;
  saddr.sin_port = 0;
  saddr.sin_addr = addr;
#ifdef HAVE_SOCKADDR_SA_LEN
  saddr.sin_len = sizeof(struct sockaddr_in);
#endif
  
  memset(&packet.icmp, 0, sizeof(packet.icmp));
  packet.icmp.icmp_type = ICMP_ECHO;
  packet.icmp.icmp_id = id;
  for (j = 0, i = 0; i < sizeof(struct icmp) / 2; i++)
    j += ((u16 *)&packet.icmp)[i];
  while (j>>16)
    j = (j & 0xffff) + (j >> 16);  
  packet.icmp.icmp_cksum = (j == 0xffff) ? j : ~j;
  
  while (sendto(fd, (char *)&packet.icmp, sizeof(struct icmp), 0, 
		(struct sockaddr *)&saddr, sizeof(saddr)) == -1 &&
	 retry_send());
  
  for (now = start = dnsmasq_time(); 
       difftime(now, start) < (float)PING_WAIT;)
    {
      struct timeval tv;
      fd_set rset, wset;
      struct sockaddr_in faddr;
      int maxfd = fd; 
      socklen_t len = sizeof(faddr);
      
      tv.tv_usec = 250000;
      tv.tv_sec = 0; 
      
      FD_ZERO(&rset);
      FD_ZERO(&wset);
      FD_SET(fd, &rset);
      set_dns_listeners(now, &rset, &maxfd);
      set_log_writer(&wset, &maxfd);
      
#ifdef HAVE_DHCP6
      if (daemon->ra_contexts)
	{
	  FD_SET(daemon->icmp6fd, &rset);
	  bump_maxfd(daemon->icmp6fd, &maxfd); 
	}
#endif
      
      if (select(maxfd+1, &rset, &wset, NULL, &tv) < 0)
	{
	  FD_ZERO(&rset);
	  FD_ZERO(&wset);
	}

      now = dnsmasq_time();

      check_log_writer(&wset);
      check_dns_listeners(&rset, now);

#ifdef HAVE_DHCP6
      if (daemon->ra_contexts && FD_ISSET(daemon->icmp6fd, &rset))
	icmp6_packet();
#endif
      
#ifdef HAVE_TFTP
      check_tftp_listeners(&rset, now);
#endif

      if (FD_ISSET(fd, &rset) &&
	  recvfrom(fd, &packet, sizeof(packet), 0,
		   (struct sockaddr *)&faddr, &len) == sizeof(packet) &&
	  saddr.sin_addr.s_addr == faddr.sin_addr.s_addr &&
	  packet.icmp.icmp_type == ICMP_ECHOREPLY &&
	  packet.icmp.icmp_seq == 0 &&
	  packet.icmp.icmp_id == id)
	{
	  gotreply = 1;
	  break;
	}
    }
  
#if defined(HAVE_LINUX_NETWORK) || defined(HAVE_SOLARIS_NETWORK)
  close(fd);
#else
  opt = 1;
  setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &opt, sizeof(opt));
#endif

  return gotreply;
}
#endif

 

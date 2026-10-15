#include <hubble/errno.h>
#include <hubble/string.h>
#include <hubble/syscalls.h>

/* Linux's struct utsname: six NUL-terminated fields of 65 bytes. */
#define UTS_LEN 65

struct utsname {
  char sysname[UTS_LEN];
  char nodename[UTS_LEN]; /* host name: fixed, there is no sethostname */
  char release[UTS_LEN];
  char version[UTS_LEN];
  char machine[UTS_LEN];
  char domainname[UTS_LEN];
};

long sys_uname(struct utsname *buf) {
  if (!buf)
    return -EFAULT;
  struct utsname u;
  memset(&u, 0, sizeof(u));
  strcpy(u.sysname, "HubbleOS");
  strcpy(u.nodename, "hubble");
  strcpy(u.release, "0.0.0");
  strcpy(u.version, "#1");
  strcpy(u.machine, "x86_64");
  strcpy(u.domainname, "(none)");
  memcpy(buf, &u, sizeof(u));
  return 0;
}

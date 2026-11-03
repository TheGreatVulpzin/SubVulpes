/* The version of this server and the check of server_min. This is not a public SDK header. */
#ifndef SV_SERVER_VERSION_H
#define SV_SERVER_VERSION_H

/* The build sets it from the release tag, such as "0.2.0" or "0.2.0-rc.1". A development build has none. */
#ifndef SV_SERVER_VERSION
#define SV_SERVER_VERSION ""
#endif

/* Nonzero when a server of version `have` meets the server_min of a plugin. A suffix such as -rc.1 does not count.
 * An empty version is a development build, and an empty server_min asks for nothing. Both accept. */
int sv_server_version_ok(const char *have, const char *want);

#endif

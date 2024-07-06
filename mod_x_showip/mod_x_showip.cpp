// *
// * ShowIP :: Show client IP as text. (Supported X-Forwarded-For)
// * (C)2018-2023 Project Entertainments*
// * http://n.xprj.net/
// *
#include "../Include/Apache24Modules.h"

static int x_handler(request_rec *r) {
	//Handler
	int EnableXFF = 0;
	if(r->handler[0] != 'x') { return DECLINED; } //Fast decline
	if(strcmp(r->handler, "x-showip")) {
		if(!strcmp(r->handler, "x-showip-xff")) {
			EnableXFF = 1;
		} else {
			return DECLINED;
		}
	}
	//End of Handler

	//Get IP
	char *IP	   = 0;
	char  IPVersion[4] = { '4', 0x00, 0x00, 0x00 };
	if(EnableXFF == 1) {
		apr_table_t *TableXFFIP = apr_table_make(r->pool, 8);
		apr_table_add(TableXFFIP, "X-Forwarded-For", apr_table_get(r->headers_in, "X-Forwarded-For")); //Copy string to local table
		IP = (char *) apr_table_get(TableXFFIP, "X-Forwarded-For");
		for(unsigned int i = 0; i < 0xFF; i++) {
			if(IP[i] == 0x00) { break; } //No ','
			if(IP[i] == ',') {
				IP[i] = 0x00;
				break;
			}					 //1st ','
			if(IP[i] == ':') { IPVersion[0] = '6'; } //IPv6
		}
	} else {
		IP = r->useragent_ip;
		for(unsigned int i = 0; i < 0xFF; i++) {
			if(r->useragent_ip[i] == 0x00) { break; }
			if(r->useragent_ip[i] == '.') {
				IPVersion[0] = '4';
				break;
			} //Fast break
			if(r->useragent_ip[i] == ':') {
				IPVersion[0] = '6';
				break;
			} //IPv6
		}
	}

	//Headers
	apr_table_t *header_out = apr_table_make(r->pool, 8);
	apr_table_add(header_out, "Content-Type", "text/plain");
	apr_table_add(header_out, "Expires", "Thu, 01 Jan 1970 00:00:00 GMT");
	apr_table_add(header_out, "Cache-Control", "private, must-revalidate, max-age=0, s-maxage=0");
	apr_table_add(header_out, "X-PoweredBy", "mod_x_showip [http://n.xprj.net/]");
	apr_table_add(header_out, "X-IP", IP);
	apr_table_add(header_out, "X-IP-Version", IPVersion);
	r->headers_out	= header_out;
	r->content_type = apr_table_get(header_out, "Content-Type");

	//ap_rprintf(r, "%s", );
	ap_rputs(IP, r);

	return OK;
}

static void register_hooks(apr_pool_t *p) { ap_hook_handler(x_handler, 0, 0, APR_HOOK_FIRST); }

AP_DECLARE_MODULE(x_showip) = {
	STANDARD20_MODULE_STUFF,
	0,			/* create per-directory config structure */
	0,			/* merge per-directory config structures */
	0,			/* create per-server config structure */
	0,			/* merge per-server config structures */
	0,			/* command apr_table_t */
	register_hooks		/* register hooks */
};

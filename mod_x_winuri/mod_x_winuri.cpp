// *
// * Windows Illegal Path Converter Module (mod_x_winuri) :: Avoid 403 error when URI contains ':' and '|'.
// * (C)2018-2023 Project Entertainments*
// * http://n.xprj.net/
// *
#include "../Include/Apache24Modules.h"

static int x_handler(request_rec *r){
	if(r->uri[0] != '/' && r->uri[0] != '\0') {
		return DECLINED;
	}

	//Convert ' : -> # ' and ' | -> # '
	for(unsigned int i = 0; i < 0xFFFFFFFF; i++) {
		if(r->uri[i] == 0x00) { break; }	  //I trust Apache core, It will certainly add '\0' to string suffix.
		if(r->uri[i] == ':') { r->uri[i] = '#'; } // '#' will never be passed from clients. It is handled by browser internal process.
		if(r->uri[i] == '|') { r->uri[i] = '#'; }
	}

	return DECLINED; //It just cheats the Apache core. It really does not change. (If use OK, will really access '#' converted path and will get error.)
}

static void register_hooks(apr_pool_t *p){ ap_hook_translate_name(x_handler, 0, 0, APR_HOOK_FIRST); }

AP_DECLARE_MODULE(x_winuri) = {
	STANDARD20_MODULE_STUFF,
	NULL,			/* create per-directory config structure */
	NULL,			/* merge per-directory config structures */
	NULL,			/* create per-server config structure */
	NULL,			/* merge per-server config structures */
	NULL,			/* command apr_table_t */
	register_hooks		/* register hooks */
};

/* © 2017 Chris Young <chris@unsatisfactorysoftware.co.uk> */

#include <stdio.h>
#include <curl/curl.h>
#include <jsmn.h>

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>

#include "token.h"
#include "gcp_dev/devbase.h"
#include "token_priv.h"

#if 1
static char *access_token = NULL;
static char *refresh_token = NULL;

static ULONG access_token_expiry = 0;
#else
#define access_token db->access_token
#define refresh_token db->refresh_token
#define access_token_expiry db->access_token_expiry
#endif

static const char client_id[] = PRIV_CLIENT_ID;
static const char client_secret[] = PRIV_CLIENT_SECRET;

static struct DeviceBase *db = NULL;

struct coutbuf {
	char *buffer;
	int posn;
	int size;
};

static uint write_cb(char *in, uint size, uint nmemb, struct coutbuf *p)
{
  uint r;
  r = size * nmemb;
  if((r + p->posn) > p->size) return 0;
  memcpy(p->buffer + p->posn, in, r);
  p->posn += r;
  return r;
}

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
	if (tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
			strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
		return 0;
	}
	return -1;
}

static int parse_json_printer_list(char *json_str, char **names, char **ids, uint32 size)
{
	int r;
	int i;
	int j = 0;
	jsmn_parser p;
	jsmntok_t t[1024]; /* We expect no more than 128 tokens */
	char *id = NULL;
	char *display_name = NULL;
	
	jsmn_init(&p);
	
	r = jsmn_parse(&p, json_str, strlen(json_str), t, sizeof(t)/sizeof(t[0]));
	if (r < 0) {
		printf("Failed to parse JSON: %d\n", r);
		return ERR_NOMEM;
	}

	/* Assume the top-level element is an object */
	if (r < 1 || t[0].type != JSMN_OBJECT) {
		printf("Object expected\n");
		return ERR_NOMEM;
	}

	/* Loop over all keys of the root object */
	for (i = 1; i < r; i++) {
		if (jsoneq(json_str, &t[i], "id") == 0) {
			/* We may use strndup() to fetch string value */
			id = strndup(json_str + t[i+1].start, t[i+1].end-t[i+1].start);
	//		printf("id: %s\n", id);
			if(names[j] != NULL) ids[j] = ASPrintf("%s", id);
			free(id);
			j++;
			if(j > size) return ERR_NOMEM;
			i++;
		} else if (jsoneq(json_str, &t[i], "displayName") == 0) {
			/* We may use strndup() to fetch string value */
			display_name = strndup(json_str + t[i+1].start, t[i+1].end-t[i+1].start);
	//		printf("displayName: %s\n", display_name);
			if(names[j] == NULL) names[j] = ASPrintf("%s", display_name);
			free(display_name);
			i++;
		}
	}
	
	return ERR_OK;
}

static void parse_json(char *json_str)
{
	int r;
	int i;
	jsmn_parser p;
	jsmntok_t t[128]; /* We expect no more than 128 tokens */
	ULONG micros;

	jsmn_init(&p);
	
	r = jsmn_parse(&p, json_str, strlen(json_str), t, sizeof(t)/sizeof(t[0]));
	if (r < 0) {
		printf("Failed to parse JSON: %d\n", r);
		return;
	}

	/* Assume the top-level element is an object */
	if (r < 1 || t[0].type != JSMN_OBJECT) {
		printf("Object expected\n");
		return;
	}

	/* Loop over all keys of the root object */
	for (i = 1; i < r; i++) {
		if (jsoneq(json_str, &t[i], "access_token") == 0) {
			/* We may use strndup() to fetch string value */
			access_token = strndup(json_str + t[i+1].start, t[i+1].end-t[i+1].start);
//			printf("access_token: %s\n", access_token);
			i++;
		} else if (jsoneq(json_str, &t[i], "refresh_token") == 0) {
			/* We may use strndup() to fetch string value */
			refresh_token = strndup(json_str + t[i+1].start, t[i+1].end-t[i+1].start);
//			printf("refresh_token: %s\n", refresh_token);
			SetVar("gcp.device/token", refresh_token, t[i+1].end-t[i+1].start, GVF_GLOBAL_ONLY | GVF_SAVE_VAR);
			i++;
		} else if (jsoneq(json_str, &t[i], "expires_in") == 0) {
			/* We may want to do strtol() here to get numeric value */
			CurrentTime(&access_token_expiry, &micros);
			access_token_expiry += strtol(json_str + t[i+1].start, NULL, 0);
//			printf("exp_in: %ld\n", exp_time);
			i++;
		}
	}
}

static BOOL parse_json_success(char *json_str)
{
	int r;
	int i;
	jsmn_parser p;
	jsmntok_t t[128]; /* We expect no more than 128 tokens */
	BOOL success = TRUE;
	char *message = NULL;

	jsmn_init(&p);
	
	r = jsmn_parse(&p, json_str, strlen(json_str), t, sizeof(t)/sizeof(t[0]));
	if (r < 0) {
		printf("Failed to parse JSON: %d\n", r);
		return FALSE;
	}

	/* Assume the top-level element is an object */
	if (r < 1 || t[0].type != JSMN_OBJECT) {
		printf("Object expected\n");
		return FALSE;
	}

	/* Loop over all keys of the root object */
	for (i = 1; i < r; i++) {
		if (jsoneq(json_str, &t[i], "success") == 0) {
			/* We may use strndup() to fetch string value */
			if(*(json_str + t[i+1].start) == 'f') {
				success = FALSE;
			} else {
				success = TRUE;
			}
			i++;
		} else if (jsoneq(json_str, &t[i], "message") == 0) {
			if(success == FALSE) {
				/* We may use strndup() to fetch string value */
				if(message = strndup(json_str + t[i+1].start, t[i+1].end-t[i+1].start)) {
					TimedDosRequesterTags(TDR_FormatString, message,
							TDR_TitleString, "gcp.device",
							TDR_GadgetString, "OK",
							TDR_ImageType, TDRIMAGE_ERROR,
							TAG_DONE);
					free(message);
				}
			}
			i++;
		}
	}
	return success;
}

static void poll_token(const char *device_code)
{
  CURL *curl;
  CURLcode res;
  struct coutbuf cb;
  char *buffer = calloc(1024, 1);
  cb.buffer = buffer;
  cb.posn = 0;
  cb.size = 1024;
   
	char *post_req = ASPrintf("client_id=%s&client_secret=%s&code=%s&grant_type=http://oauth.net/grant_type/device/1.0", client_id, client_secret, device_code);

	curl = curl_easy_init();

	if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL, "https://www.googleapis.com/oauth2/v4/token");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); /* probably should fix this */
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_req);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &cb);

    /* Perform the request, res will get the return code */
    res = curl_easy_perform(curl);
    /* Check for errors */
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));

    /* always cleanup */
    curl_easy_cleanup(curl);
  }
  
  FreeVec(post_req);
	  
//  printf("%s\n", buffer);

	parse_json(buffer);

 	free(buffer);
  
}

static void parse_json_show_code(char *json_str)
{
	int r;
	int i;
	jsmn_parser p;
	jsmntok_t t[128]; /* We expect no more than 128 tokens */
	
	uint exp_time, interval;
	char *verification_url = NULL;
	char *user_code = NULL;
	char *device_code = NULL;

	jsmn_init(&p);
	
	r = jsmn_parse(&p, json_str, strlen(json_str), t, sizeof(t)/sizeof(t[0]));
	if (r < 0) {
		printf("Failed to parse JSON: %d\n", r);
		return;
	}

	/* Assume the top-level element is an object */
	if (r < 1 || t[0].type != JSMN_OBJECT) {
		printf("Object expected\n");
		return;
	}

	/* Loop over all keys of the root object */
	for (i = 1; i < r; i++) {
		if (jsoneq(json_str, &t[i], "verification_url") == 0) {
			/* We may use strndup() to fetch string value */
			verification_url = strndup(json_str + t[i+1].start, t[i+1].end-t[i+1].start);
			i++;
		} else if (jsoneq(json_str, &t[i], "user_code") == 0) {
			/* We may use strndup() to fetch string value */
			user_code = strndup(json_str + t[i+1].start, t[i+1].end-t[i+1].start);
			i++;
		} else if (jsoneq(json_str, &t[i], "device_code") == 0) {
			/* We may use strndup() to fetch string value */
			device_code = strndup(json_str + t[i+1].start, t[i+1].end-t[i+1].start);
			i++;
		} else if (jsoneq(json_str, &t[i], "interval") == 0) {
			/* We may want to do strtol() here to get numeric value */
			interval = strtol(json_str + t[i+1].start, NULL, 0);
//			printf("exp_in: %ld\n", exp_time);
			i++;
		} else if (jsoneq(json_str, &t[i], "expires_in") == 0) {
			/* We may want to do strtol() here to get numeric value */
			exp_time = strtol(json_str + t[i+1].start, NULL, 0);
//			printf("exp_in: %ld\n", exp_time);
			i++;
		}
	}
	
	if(verification_url && user_code && device_code) {
		/* got everything we need */
		/* this should loop until complete */
		int ret = 0;

		while((refresh_token == NULL) && (exp_time > 0)) {
			ret = TimedDosRequesterTags(TDR_FormatString, "Please access %s\nand enter the code %s",
						TDR_TitleString, "gcp.device",
						TDR_GadgetString, "Cancel",
						TDR_Arg1, verification_url,
						TDR_Arg2, user_code,
						TDR_Timeout, interval,
						TDR_Inactive, TRUE,
						TAG_DONE);

			if(ret > -1) break;
										
			poll_token(device_code); /* we should be checking for errors properly */
			exp_time -= interval;
		}
	
		free(device_code);	
		free(verification_url);
		free(user_code);
	}
}

static void new_token(void)
{
  struct coutbuf cb;
  char *buffer = calloc(1024, 1);
  cb.buffer = buffer;
  cb.posn = 0;
  cb.size = 1024;

  CURL *curl;
  CURLcode res;
  char *device_code;
	char *post_req = ASPrintf("client_id=%s&scope=https://www.googleapis.com/auth/cloudprint", client_id);

	curl = curl_easy_init();

	if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL, "https://accounts.google.com/o/oauth2/device/code");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); /* probably should fix this */
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_req);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &cb);

    /* Perform the request, res will get the return code */
    res = curl_easy_perform(curl);
    /* Check for errors */
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));

    /* always cleanup */
    curl_easy_cleanup(curl);
  }
  
   FreeVec(post_req);

//	printf("%s\n\n", buffer);
	parse_json_show_code(buffer);
	free(buffer);
}


static void refresh_old_token(const char *r_token)
{
  CURL *curl;
  CURLcode res;
  struct coutbuf cb;
  char *buffer = calloc(1024, 1);
  cb.buffer = buffer;
  cb.posn = 0;
  cb.size = 1024;
 
	char *post_req = ASPrintf("client_id=%s&client_secret=%s&refresh_token=%s&grant_type=refresh_token", client_id, client_secret, r_token);

	curl = curl_easy_init();

	if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL, "https://www.googleapis.com/oauth2/v4/token");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); /* probably should fix this */
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_req);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &cb);

    /* Perform the request, res will get the return code */
    res = curl_easy_perform(curl);
    /* Check for errors */
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));

    /* always cleanup */
    curl_easy_cleanup(curl);
  }
  
  FreeVec(post_req);
	  
//  printf("%s\n", buffer);

	parse_json(buffer);

 	free(buffer);
  
}

/** submit print job **/
static int submit_job(const char *printer_id, const char *file)
{
	char ticket[] = "{\n\"version\": \"1.0\",\n\"print\": {}\n}";
 
    CURL *curl;
  CURLcode res;
  struct coutbuf cb;
  char *buffer = calloc(10240, 1);
  cb.buffer = buffer;
  cb.posn = 0;
  cb.size = 10240;
  BOOL success = FALSE;
  long http_code = 0;
  struct curl_httppost *formpost=NULL;
  struct curl_httppost *lastptr=NULL;
  struct curl_slist *headerlist=NULL;
  static const char buf[] = "Expect:";
	char *auth = ASPrintf("Authorization: Bearer %s", access_token);


 // curl_global_init(CURL_GLOBAL_ALL);

  /* Fill in the file upload field */
  curl_formadd(&formpost,
               &lastptr,
               CURLFORM_COPYNAME, "content",
               CURLFORM_FILE, file,
               CURLFORM_END);

  /* Fill in the filename field */
  curl_formadd(&formpost,
               &lastptr,
               CURLFORM_COPYNAME, "title",
               CURLFORM_COPYCONTENTS, file,
               CURLFORM_END);

  curl_formadd(&formpost,
               &lastptr,
               CURLFORM_COPYNAME, "printerid",
               CURLFORM_COPYCONTENTS, printer_id,
               CURLFORM_END);

  curl_formadd(&formpost,
               &lastptr,
               CURLFORM_COPYNAME, "ticket",
               CURLFORM_COPYCONTENTS, ticket, //FILECONTENT
               CURLFORM_END);


	curl = curl_easy_init();
  /* initialize custom header list (stating that Expect: 100-continue is not
     wanted */
  headerlist = curl_slist_append(headerlist, buf);
  headerlist = curl_slist_append(headerlist, auth);

	if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL, "https://www.google.com/cloudprint/submit");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
    curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); /* probably should fix this */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &cb);


    /* Perform the request, res will get the return code */
    res = curl_easy_perform(curl);
    /* Check for errors */
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));

	curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);

    /* always cleanup */
    curl_easy_cleanup(curl);
    /* then cleanup the formpost chain */
    curl_formfree(formpost);
    /* free slist */
    curl_slist_free_all(headerlist);

  }
  
  FreeVec(auth);

	if(http_code == 200) {
		success = parse_json_success(buffer);
	} else {
		success = FALSE;

		TimedDosRequesterTags(TDR_FormatString, "Server returned HTTP error %ld",
							TDR_TitleString, "gcp.device",
							TDR_GadgetString, "OK",
							TDR_ImageType, TDRIMAGE_ERROR,
							TDR_Arg1, http_code,
							TAG_DONE);
	}

	DebugPrintF("%s\n", buffer);

	free(buffer);

	if(success == TRUE) {
		return ERR_OK;
	} else {
		return ERR_PRINT;
	}
}

static unsigned long get_task_id(void)
{
	return (unsigned long)FindTask(NULL);
}

static int auth(void)
{
	ULONG cur_time, micros;

//	CRYPTO_set_id_callback((unsigned long (*)())get_task_id);

	CurrentTime(&cur_time, &micros);

	if((access_token != NULL) && (access_token_expiry > cur_time)) {
		/* token still valid */
		return ERR_OK;
	}

//	curl_global_init(CURL_GLOBAL_ALL);

	refresh_token = malloc(100);
	int res = GetVar("gcp.device/token", refresh_token, 100, GVF_GLOBAL_ONLY);

	if(res == -1) {
		free(refresh_token);
		refresh_token = NULL;
		new_token();
	} else {
		refresh_old_token(refresh_token);
		
		if(access_token == NULL) {
			free(refresh_token);
			refresh_token = NULL;
			new_token();
		}
	}

	if(access_token == NULL) return ERR_NO_AUTH;

	return ERR_OK;
}

static int list_printers_internal(char **names, char **ids, uint32 size)
{
	int err = auth();
	if(err != ERR_OK) return err;
 
    CURL *curl;
  CURLcode res;
  struct coutbuf cb;
  char *buffer = calloc(102400, 1);
  cb.buffer = buffer;
  cb.posn = 0;
  cb.size = 102400;

  struct curl_slist *headerlist=NULL;
	char *authh = ASPrintf("Authorization: Bearer %s", access_token);

 	curl = curl_easy_init();
  headerlist = curl_slist_append(headerlist, authh);

	if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL, "https://www.google.com/cloudprint/search");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
   curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); /* probably should fix this */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &cb);

    /* Perform the request, res will get the return code */
    res = curl_easy_perform(curl); // <-- here be crash
    /* Check for errors */
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));

    /* always cleanup */
    curl_easy_cleanup(curl);
    /* free slist */
    curl_slist_free_all(headerlist);

  }
  
  FreeVec(authh);
	  
 // printf("%s\n", buffer);

	err = parse_json_printer_list(buffer, names, ids, size);

 	free(buffer);

 	return err;  
}

static int print_file_internal(int unit, const char *filename)
{
	char printer_id[100];
	int res;

	int err = auth();
	if(err != ERR_OK) return err;
	
	char *envvar = ASPrintf("gcp.device/%ld", unit);
	
	res = GetVar(envvar, printer_id, 100, GVF_GLOBAL_ONLY);
	if(res == -1) return ERR_NO_PRINTER;
	
	FreeVec(envvar);

	return submit_job(printer_id, filename);
}

int print_file(struct DeviceBase *devb, int unit, const char *filename)
{
	db = devb;

	return print_file_internal(unit, filename); // show error message here if necessary
}

int list_printers(struct DeviceBase *devb, char **names, char **ids, uint32 size)
{
	db = devb;

	return list_printers_internal(names, ids,size);
}

void cleanup(struct DeviceBase *devb)
{
	if(access_token != NULL) free(access_token);
	if(refresh_token != NULL) free(refresh_token);
	
	access_token = NULL;
	refresh_token = NULL;
	
//	curl_global_cleanup();
}

#ifdef STANDALONE
int main()
{
	refresh_token = malloc(100);
	int res = GetVar("gcp.device/token", refresh_token, 100, GVF_GLOBAL_ONLY);

	if(res == -1) {
		free(refresh_token);
		refresh_token = NULL;
		new_token();
	} else {
		refresh_old_token(refresh_token);
		
		if(access_token == NULL) {
			free(refresh_token);
			refresh_token = NULL;
			new_token();
		}
	}

	if(access_token == NULL) return 5;
	
	list_printers();
//	submit_job("f7502437-b986-7fe6-ecd2-e58da4fec0cb", "ram:test.ps");

	if(access_token != NULL) free(access_token);
	if(refresh_token != NULL) free(refresh_token);
	
	return 0;
}
#endif

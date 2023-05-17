#include "httpd.h"
#include <sys/stat.h>
#include <security/pam_appl.h>
#include <security/pam_misc.h>

#define CHUNK_SIZE 1024 // read 1024 bytes at a time

// Public directory settings
#define PUBLIC_DIR "/var/www/webroot"
#define INDEX_HTML "/index.html"
#define NOT_FOUND_HTML "/404.html"
#define AUTH_PATH "/private"
#define SERVICE "courseWorkService"

int main(int c, char **v) {
  char *port = c == 1 ? "8000" : v[1];
  serve_forever(port);
  return 0;
}

static int pam_conversation(int num_msg, const struct pam_message **msg,
                            struct pam_response **resp, void *appdata_ptr) {
    char *pass = malloc(strlen(appdata_ptr) + 1);
    strcpy(pass, appdata_ptr);

    int i;

    *resp = calloc(num_msg, sizeof(struct pam_response));

    for (i = 0; i < num_msg; ++i) {
        /* Ignore all PAM messages except prompting for hidden input */
        if (msg[i]->msg_style != PAM_PROMPT_ECHO_OFF)
            continue;

        /* Assume PAM is only prompting for the password as hidden input */
        resp[i]->resp = pass;
    }

    return PAM_SUCCESS;
}

int file_exists(const char *file_name) {
  struct stat buffer;
  int exists;

  exists = (stat(file_name, &buffer) == 0);

  return exists;
}

int read_file(const char *file_name) {
  char buf[CHUNK_SIZE];
  FILE *file;
  size_t nread;
  int err = 1;

  file = fopen(file_name, "r");

  if (file) {
    while ((nread = fread(buf, 1, sizeof buf, file)) > 0)
      fwrite(buf, 1, nread, stdout);

    err = ferror(file);
    fclose(file);
  }
  return err;
}

void route(char *login, char *pass) {
  ROUTE_START()

  GET("/") {
    char index_html[20];
    sprintf(index_html, "%s%s", PUBLIC_DIR, INDEX_HTML);

    HTTP_200;
    if (file_exists(index_html)) {
      read_file(index_html);
    } else {
      printf("Hello! You are using %s\n\n", request_header("User-Agent"));
    }
  }

  GET("/test") {
    HTTP_200;
    printf("List of request headers:\n\n");

    header_t *h = request_headers();

    while (h->name) {
      printf("%s: %s\n", h->name, h->value);
      h++;
    }
  }

  POST("/") {
    HTTP_201;
    printf("Wow, seems that you POSTed %d bytes.\n", payload_size);
    printf("Fetch the data using `payload` variable.\n");
    if (payload_size > 0)
      printf("Request body: %s", payload);
  }

  GET(uri) {
    char file_name[255];
    sprintf(file_name, "%s%s", PUBLIC_DIR, uri);

    char *tmpStr;
    char isPrivate = 'n';
    for(int tmpDel = 1; tmpDel < strlen(uri); tmpDel++) {
      if(uri[tmpDel] == '/') {
        tmpStr = malloc(tmpDel + 1);
        strncpy(tmpStr, uri, tmpDel);

        if(!strcmp(tmpStr, AUTH_PATH)) {
          isPrivate = 'y';
        }
        break;
      }
    }

    if (isPrivate == 'y') {
      if (login == NULL || pass == NULL) {
        HTTP_401;
      }
      else {
        struct pam_conv conv = {&pam_conversation, (void *) pass};
          pam_handle_t *handle;
          int startResult, authResult;
          startResult = pam_start(SERVICE, login, &conv, &handle); 
          if (startResult != PAM_SUCCESS) {
            return 1;
          }
          
          authResult = pam_authenticate(handle, PAM_SILENT | PAM_DISALLOW_NULL_AUTHTOK);
          if (authResult != PAM_SUCCESS) {
            return 1;
          }
          pam_end(handle, authResult);

          if (file_exists(file_name)) {
            HTTP_200;
            read_file(file_name);
          } else {
            HTTP_404;
            sprintf(file_name, "%s%s", PUBLIC_DIR, NOT_FOUND_HTML);
            if (file_exists(file_name))
              read_file(file_name);
          }
      }
    }
    else {
      if (file_exists(file_name)) {
        HTTP_200;
        read_file(file_name);
      } else {
        HTTP_404;
        sprintf(file_name, "%s%s", PUBLIC_DIR, NOT_FOUND_HTML);
        if (file_exists(file_name))
          read_file(file_name);
      }
    }
  }

  ROUTE_END()
}
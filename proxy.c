#include <stdio.h>
#include "csapp.h"

#define MAX_OBJECT_SIZE 7204056
/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
  "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

// Part 3
// type = 0, not a range request
// type = 1, range request of the form r1-r2
// type = 2, range request of the form r1-
// type = 3, range request of the form -r1
typedef struct rangeNode
{
  int type;
  int first;
  int second;
} rangeNode;

// Part 1
void doit (int fd);
void read_requesthdrs (rio_t * rp, rangeNode * nodePtr);
int parse_uri (char *uri, char *hostname, int *port);
void clienterror (int fd, char *cause, char *errnum, char *shortmsg,
		  char *longmsg);
int powerten (int i);
static void request_hdr (char *buf, char *buf2ser, char *hostname);

// Part 2
void init_cache (void);
void free_cache (void);
static int load_cache (char *tag);
static void save_cache (char *response, int len);
char *strrstr (char *s, char *str);

struct cache
{
  int len;
  int pkg_hdr_len;
  char *pkg_hdr;
  char *tag;
  char *block;
};
static struct cache cache;

int
main (int argc, char **argv)
{
  signal (SIGPIPE, SIG_IGN);	// ignore sigpipe

  //printf("%s", user_agent_hdr);
  int listenfd;
  int connfd;

  socklen_t clientlen;
  struct sockaddr_in clientaddr;

  // how many argcs
  if (argc != 2)
    {
      fprintf (stderr, "usage: %s <port>\n", argv[0]);
      exit (1);
    }

  init_cache ();
  listenfd = Open_listenfd (argv[1]);
  while (1)
    {
      clientlen = sizeof (clientaddr);
      connfd = Accept (listenfd, (SA *) & clientaddr, &clientlen);
      doit (connfd);
      close (connfd);
    }
  return 0;
}

// Initialize the cache, malloc space for the cache
void
init_cache (void)
{
  cache.len = 0;
  cache.pkg_hdr_len = 0;
  cache.pkg_hdr = malloc (MAXLINE);
  cache.tag = malloc (MAXLINE);
  cache.block = malloc (MAX_OBJECT_SIZE);
}

// Free cache
void
free_cache (void)
{
  free (cache.block);
  free (cache.tag);
  free (cache.pkg_hdr);
}

// load_cache - load data from cache, search desired cache and buffer the data in response
static int
load_cache (char *tag)
{
  if (strcmp (cache.tag, tag) == 0)
    {
      return 1;
    }
  else
    {
      return 0;
    }
}

// save_cache - save data from server in cache, copy response and tag into cache
static void
save_cache (char *response, int len)
{
  // copy response and tag into cache
  cache.len = len;
  memcpy (cache.block, response, len);
}

// strrstr-find the last time that str in s
char *
strrstr (char *s, char *str)
{
  char *p;
  int len = strlen (s);
  for (p = s + len - 1; p >= s; p--)
    {
      if ((*p == *str) && (memcmp (p, str, strlen (str)) == 0))
	return p;
    }
  return NULL;
}

// doit - handle one HTTP request/response transaction
void
doit (int fd)
{
  int serverfd, len, object_len;

  int *port;
  char port2[10];
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char cache_buf[MAX_OBJECT_SIZE];	// store cache data stuff
  char filename[MAXLINE];	// client request filename
  char hostname[MAXBUF];	// client request hostname
  char buf2ser[MAXLINE];	// proxy to server
  char ser_response[MAXLINE];	// server to proxy
  rio_t rio, rio_ser;		// rio: between client and proxy
  // rio_ser: between proxy and server
  rangeNode range = { 0, 0, 0 };
  port = malloc (sizeof (int));
  *port = 80;			// default port 80

  memset (buf2ser, 0, sizeof (buf2ser));
  memset (filename, 0, sizeof (filename));
  memset (hostname, 0, sizeof (hostname));
  memset (ser_response, 0, sizeof (ser_response));
  memset (uri, 0, sizeof (uri));
  memset (method, 0, sizeof (method));
  memset (buf, 0, sizeof (buf));
  memset (version, 0, sizeof (version));
  memset (cache_buf, 0, sizeof (cache_buf));

  // step1: obtain request from client and parse the request
  Rio_readinitb (&rio, fd);
  if (!Rio_readlineb (&rio, buf, MAXLINE))
    return;
  //printf("request from client: %s", buf);

  // parse request into method, uri, version
  sscanf (buf, "%s %s %s", method, uri, version);

  // check HTTP version, if 1.1, change it into 1.0
  if (!strcasecmp (version, "HTTP/1.1"))
    {
      strcpy (version, "HTTP/1.0");
    }

  // we only need GET method
  if (strcasecmp (method, "GET"))
    {
      clienterror (fd, method, "501", "Not Implemented",
		   "This proxy does not implement this method");
      return;
    }
  read_requesthdrs (&rio, &range);

  /* Parse URI from GET request */
  parse_uri (uri, hostname, port);
  strcpy (filename, uri);
  sprintf (buf2ser, "%s %s %s\r\n", method, filename, version);
  //printf("proxy to server: %s\n", buf2ser);

  // request header
  request_hdr (buf, buf2ser, hostname);

  // Check cache first
  if (load_cache (uri) == 1)
    {
      printf ("Hit!\n");
      if (rio_writen (fd, cache.pkg_hdr, cache.pkg_hdr_len) !=
	  cache.pkg_hdr_len)
	{
	  fprintf (stderr, "Error: cache load!\n");
	  return;
	}

      if (range.type == 0)
	{
	  if (rio_writen (fd, cache.block, cache.len) != cache.len)
	    {
	      fprintf (stderr, "Error: cache load!\n");
	      return;
	    }
	}
      if (range.type == 1)
	{
	  if (range.first >= cache.len)
	    {
	      return;
	    }
	  else
	    {
	      Rio_writen (fd, "\n", 1);
	      char *start_ptr = cache.block + range.first;
	      if (range.second < cache.len)
		{
		  if (rio_writen
		      (fd, start_ptr,
		       range.second - range.first + 1) !=
		      (range.second - range.first + 1))
		    {
		      fprintf (stderr, "Error: cache load!\n");
		      return;
		    }
		}
	      else
		{
		  if (rio_writen (fd, start_ptr, cache.len - range.first) !=
		      (cache.len - range.first))
		    {
		      fprintf (stderr, "Error: cache load!\n");
		      return;
		    }
		}
	    }
	}
      if (range.type == 2)
	{
	  if (range.first >= cache.len)
	    {
	      return;
	    }
	  else
	    {
	      Rio_writen (fd, "\n", 1);
	      char *start_ptr = cache.block + range.first;
	      printf ("content_len:\n%d\n", cache.len - range.first);
	      printf ("content_buf:\n%s\n", start_ptr);
	      if (rio_writen (fd, start_ptr, cache.len - range.first) !=
		  (cache.len - range.first))
		{
		  fprintf (stderr, "Error: cache load!\n");
		  return;
		}
	    }
	}
      if (range.type == 3)
	{
	  if (-(range.first) >= cache.len)
	    {
	      if (rio_writen (fd, cache.block, cache.len) != cache.len)
		{
		  fprintf (stderr, "Error: cache load!\n");
		  return;
		}
	    }
	  else
	    {
	      Rio_writen (fd, "\n", 1);
	      char *start_ptr = cache.block + cache.len + range.first;
	      printf ("content_len:\n%d\n", -range.first);
	      printf ("content_buf:\n%s\n", start_ptr);
	      if (rio_writen (fd, start_ptr, -(range.first)) != -range.first)
		{
		  fprintf (stderr, "Error: cache load!\n");
		  return;
		}
	    }
	}
    }
  else
    {
      // if cache miss then forward the request to server
      // from proxy to server
      sprintf (port2, "%d", *port);
      if ((serverfd = open_clientfd (hostname, port2)) < 0)
	{
	  fprintf (stderr, "open server fd error\n");
	  return;
	}

      Rio_readinitb (&rio_ser, serverfd);
      // send request to server
      Rio_writen (serverfd, buf2ser, strlen (buf2ser));

      // recieve the response from the server
      memset (cache_buf, 0, sizeof (cache_buf));
      object_len = 0;
      char temp_pkg_hdr[MAXLINE];
      int pkg_hdr_flag = 0;
      int temp_pkg_hdr_len = 0;
      while ((len =
	      rio_readnb (&rio_ser, ser_response, sizeof (ser_response))) > 0)
	{
	  Rio_writen (fd, ser_response, len);
	  char temp_ser_response[MAXLINE];
	  memcpy (temp_ser_response, ser_response, ((len < 300) ? len : 300));
	  char *pkg_hdr_end = strrstr (temp_ser_response, "\r\n");
	  if ((pkg_hdr_end != NULL) && (pkg_hdr_flag == 0))
	    {
	      memcpy (temp_pkg_hdr, ser_response,
		      pkg_hdr_end - temp_ser_response + 2);
	      temp_pkg_hdr_len = pkg_hdr_end - temp_ser_response + 2;

	      int temp_object_len = object_len;
	      object_len = object_len + len - temp_pkg_hdr_len;
	      if (object_len <= MAX_OBJECT_SIZE)
		{
		  memcpy (cache_buf + temp_object_len,
			  ser_response + temp_pkg_hdr_len,
			  len - temp_pkg_hdr_len);
		}
	      pkg_hdr_flag = 1;
	    }
	  else
	    {
	      int temp_object_len = object_len;
	      object_len += len;
	      if (object_len <= MAX_OBJECT_SIZE)
		{
		  memcpy (cache_buf + temp_object_len, ser_response, len);
		}
	    }
	}
      //Rio_writen(fd, temp_pkg_hdr, temp_pkg_hdr_len);
      //Rio_writen(fd, cache_buf, object_len);
      if (object_len <= MAX_OBJECT_SIZE)
	{
	  strcpy (cache.tag, uri);
	  memcpy (cache.pkg_hdr, temp_pkg_hdr, temp_pkg_hdr_len);
	  save_cache (cache_buf, object_len);
	}
      close (serverfd);
    }
}


// request_hdr - request header
static void
request_hdr (char *buf, char *buf2ser, char *hostname)
{
  if (strcmp (buf, "Host"))
    {
      strcat (buf2ser, "Host: ");
      strcat (buf2ser, hostname);
      strcat (buf2ser, "\r\n");
    }
  if (strcmp (buf, "User-Agent:"))
    {
      strcat (buf2ser, user_agent_hdr);
    }
  if (strcmp (buf, "Proxy-Connection:"))
    {
      strcat (buf2ser, "Proxy-Connection: close\r\n");
    }
  if (strcmp (buf, "Connection:"))
    {
      strcat (buf2ser, "Connection: close\r\n");
    }
  memset (buf, 0, sizeof (buf));
  strcat (buf2ser, "\r\n");
}

// Process_range
void
process_range (char *buf, rangeNode * nodePtr)
{
  char *next_tok;
  int r1, r2;
  if ((next_tok = strstr (buf, "bytes=")) != NULL)
    {
      next_tok = next_tok + 6;
      if (sscanf (next_tok, "-%u", &r1) == 1)
	{
	  nodePtr->type = 3;
	  nodePtr->first = -r1;
	}
      else if (sscanf (next_tok, "%u-%u", &r1, &r2) == 2)
	{
	  nodePtr->type = 1;
	  nodePtr->first = r1;
	  nodePtr->second = r2;
	}
      else if (sscanf (next_tok, "%u-", &r1) == 1)
	{
	  nodePtr->type = 2;
	  nodePtr->first = r1;
	}
      else
	{
	  nodePtr->type = 0;
	  printf ("get range: error\n");
	}
    }
  printf ("range type: %d, first: %d, second: %d\n", nodePtr->type,
	  nodePtr->first, nodePtr->second);
}


// read_requesthdrs - read HTTP request headers
void
read_requesthdrs (rio_t * rp, rangeNode * nodePtr)
{
  char buf[MAXLINE];

  Rio_readlineb (rp, buf, MAXLINE);
  //printf("%s", buf);
  while (strcmp (buf, "\r\n"))
    {
      if (!strncasecmp (buf, "Range:", 6))
	{
	  process_range (buf, nodePtr);
	}
      Rio_readlineb (rp, buf, MAXLINE);
      //printf("%s", buf);
    }
  return;
}

// parse_uri - parse URI into filename and CGI args
int
parse_uri (char *uri, char *hostname, int *port)
{
  char tmp[MAXLINE];		// holds local copy of uri
  char *buf;			// ptr that traverses uri
  char *endbuf;			// ptr to end of the cmdline string
  int port_tmp[10];
  int i, j;			// loop
  char num[2];			// store port value

  buf = tmp;
  for (i = 0; i < 10; i++)
    {
      port_tmp[i] = 0;
    }
  (void) strncpy (buf, uri, MAXLINE);
  endbuf = buf + strlen (buf);
  buf += 7;			// 'http://' has 7 characters
  while (buf < endbuf)
    {
      // take host name out
      if (buf >= endbuf)
	{
	  strcpy (uri, "");
	  strcat (hostname, " ");
	  // no other character found
	  break;
	}
      if (*buf == ':')
	{			// if port number exists
	  buf++;
	  *port = 0;
	  i = 0;
	  while (*buf != '/')
	    {
	      num[0] = *buf;
	      num[1] = '\0';
	      port_tmp[i] = atoi (num);
	      buf++;
	      i++;
	    }
	  j = 0;
	  while (i > 0)
	    {
	      *port += port_tmp[j] * powerten (i - 1);
	      j++;
	      i--;
	    }
	}
      if (*buf != '/')
	{

	  sprintf (hostname, "%s%c", hostname, *buf);
	}
      else
	{			// host name done
	  strcat (hostname, "\0");
	  strcpy (uri, buf);
	  break;
	}
      buf++;
    }
  return 1;
}

// powerten - return ten to the power of i
int
powerten (int i)
{
  int num = 1;
  while (i > 0)
    {
      num *= 10;
      i--;
    }
  return num;
}

// clienterror - returns an error message to the client
void
clienterror (int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf (body, "<html><title>Tiny Error</title>");
  sprintf (body, "%s<body bgcolor=" "ffffff" ">\r\n", body);
  sprintf (body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf (body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf (body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf (buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen (fd, buf, strlen (buf));
  sprintf (buf, "Content-type: text/html\r\n");
  Rio_writen (fd, buf, strlen (buf));
  sprintf (buf, "Content-length: %d\r\n\r\n", (int) strlen (body));
  Rio_writen (fd, buf, strlen (buf));
  Rio_writen (fd, body, strlen (body));
}

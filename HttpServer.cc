/*
 * Copyright ©2022 Travis McGaha.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Pennsylvania
 * CIT 595 for use solely during Spring Semester 2022 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */
#define DEV_H_

#ifndef DEV_H_
#include <string.hpp>
#else
#include <boost/algorithm/string.hpp>
#endif

#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <sstream>

#include "./FileReader.h"
#include "./HttpConnection.h"
#include "./HttpRequest.h"
#include "./HttpUtils.h"
#include "./HttpServer.h"


using std::cerr;
using std::cout;
using std::endl;
using std::list;
using std::map;
using std::string;
using std::stringstream;
using std::unique_ptr;
using std::vector;

namespace searchserver {
///////////////////////////////////////////////////////////////////////////////
// Constants, internal helper functions
///////////////////////////////////////////////////////////////////////////////
static const char *kFivegleStr =
  "<html><head><title>595gle</title></head>\n"
  "<body>\n"
  "<center style=\"font-size:500%;\">\n"
  "<span style=\"position:relative;bottom:-0.33em;color:orange;\">5</span>"
    "<span style=\"color:red;\">9</span>"
    "<span style=\"color:gold;\">5</span>"
    "<span style=\"color:blue;\">g</span>"
    "<span style=\"color:green;\">l</span>"
    "<span style=\"color:red;\">e</span>\n"
  "</center>\n"
  "<p>\n"
  "<div style=\"height:20px;\"></div>\n"
  "<center>\n"
  "<form action=\"/query\" method=\"get\">\n"
  "<input type=\"text\" size=30 name=\"terms\" />\n"
  "<input type=\"submit\" value=\"Search\" />\n"
  "</form>\n"
  "</center><p>\n";

// static
const int HttpServer::kNumThreads = 100;

// This is the function that threads are dispatched into
// in order to process new client connections.
static void HttpServer_ThrFn(ThreadPool::Task *t);

// Given a request, produce a response.
static HttpResponse ProcessRequest(const HttpRequest &req,
                            const string &base_dir,
                            WordIndex *indices);

// Process a file request.
static HttpResponse ProcessFileRequest(const string &uri,
                                const string &base_dir);

// Process a query request.
static HttpResponse ProcessQueryRequest(const string &uri,
                                 WordIndex *index);


///////////////////////////////////////////////////////////////////////////////
// HttpServer
///////////////////////////////////////////////////////////////////////////////
bool HttpServer::run(void) {
  // Create the server listening socket.
  int listen_fd;
  cout << "  creating and binding the listening socket..." << endl;
  if (!socket_.bind_and_listen(AF_INET6, &listen_fd)) {
    cerr << endl << "Couldn't bind to the listening socket." << endl;
    return false;
  }

  // Spin, accepting connections and dispatching them.  Use a
  // threadpool to dispatch connections into their own thread.
  cout << "  accepting connections..." << endl << endl;
  ThreadPool tp(kNumThreads);
  while (1) {
    HttpServerTask *hst = new HttpServerTask(HttpServer_ThrFn);
    hst->base_dir = static_file_dir_path_;
    hst->index = index_;
    if (!socket_.accept_client(&hst->client_fd,
                    &hst->c_addr,
                    &hst->c_port,
                    &hst->c_dns,
                    &hst->s_addr,
                    &hst->s_dns)) {
      // The accept failed for some reason, so quit out of the server.
      // (Will happen when kill command is used to shut down the server.)
      break;
    }
    // The accept succeeded; dispatch it.
    tp.dispatch(hst);
  }
  return true;
}

static void HttpServer_ThrFn(ThreadPool::Task *t) {
  // Cast back our HttpServerTask structure with all of our new
  // client's information in it.
  unique_ptr<HttpServerTask> hst(static_cast<HttpServerTask *>(t));
  cout << "  client " << hst->c_dns << ":" << hst->c_port << " "
       << "(IP address " << hst->c_addr << ")" << " connected." << endl;

  // Read in the next request, process it, write the response.

  // Use the HttpConnection class to read and process the next
  // request from our current client, then write out our response.  If
  // the client sends a "Connection: close\r\n" header, then shut down
  // the connection -- we're done.
  //
  // Hint: the client can make multiple requests on our single connection,
  // so we should keep the connection open between requests rather than
  // creating/destroying the same connection repeatedly.

  bool done = false;
  HttpConnection conn(hst->client_fd);
  while (!done) {
    HttpRequest req;
    if (!conn.next_request(&req)) {
      // cannot parse request, closing connection
      done = true;
    } else {
      if (req.GetHeaderValue("connection") == "close") {
        // client wants to close connection
        done = true;
        continue;
      } else {
        HttpResponse resp = ProcessRequest(req, hst->base_dir, hst->index);
        conn.write_response(resp);
      }
    }
  }
}

static HttpResponse ProcessRequest(const HttpRequest &req,
                            const string &base_dir,
                            WordIndex *index) {
  // Is the user asking for a static file?
  if (req.uri().substr(0, 8) == "/static/") {
    return ProcessFileRequest(req.uri(), base_dir);
  }

  // The user must be asking for a query.
  return ProcessQueryRequest(req.uri(), index);
}

static HttpResponse ProcessFileRequest(const string &uri,
                                const string &base_dir) {
  // The response we'll build up.
  HttpResponse ret;

  // Steps to follow:
  //  - use the URLParser class to figure out what filename
  //    the user is asking for. Note that we identify a request
  //    as a file request if the URI starts with '/static/'
  //
  //  - use the FileReader class to read the file into memory
  //
  //  - copy the file content into the ret.body
  //
  //  - depending on the file name suffix, set the response
  //    Content-type header as appropriate, e.g.,:
  //      --> for ".html" or ".htm", set to "text/html"
  //      --> for ".jpeg" or ".jpg", set to "image/jpeg"
  //      --> for ".png", set to "image/png"
  //      etc.
  //    You should support the file types mentioned above,
  //    as well as ".txt", ".js", ".css", ".xml", ".gif",
  //    and any other extensions to get bikeapalooza
  //    to match the solution server.
  //
  // be sure to set the response code, protocol, and message
  // in the HttpResponse as well.
  string file_name = "";

  URLParser parser;
  parser.parse(uri);
  file_name = parser.path().substr(8);
  FileReader reader(file_name);
  string file_content;
  if (!is_path_safe(base_dir, file_name) || !reader.read_file(&file_content)) {
    // If you couldn't find the file, return an HTTP 404 error.
    ret.set_protocol("HTTP/1.1");
    ret.set_response_code(404);
    ret.set_message("Not Found");
    ret.AppendToBody("<html><body>Couldn't find file \""
                    + escape_html(file_name)
                    + "\"</body></html>\n");
  } else {
    // If you found the file, return an HTTP 200 OK response.
    vector<string> file_name_parts;
    boost::split(file_name_parts, file_name, boost::is_any_of("."));
    string file_type = file_name_parts[file_name_parts.size() - 1];
    if (file_type == "html" || file_type == "htm") {
      ret.set_content_type("text/html");
    } else if (file_type == "jpeg" || file_type == "jpg") {
      ret.set_content_type("image/jpeg");
    } else if (file_type == "png") {
      ret.set_content_type("image/png");
    } else if (file_type == "txt") {
      ret.set_content_type("text/plain");
    } else if (file_type == "js") {
      ret.set_content_type("text/javascript");
    } else if (file_type == "css") {
      ret.set_content_type("text/css");
    } else if (file_type == "xml") {
      ret.set_content_type("text/xml");
    } else if (file_type == "gif") {
      ret.set_content_type("image/gif");
    } else {
      ret.set_content_type("text/plain");
    }
    ret.set_protocol("HTTP/1.1");
    ret.set_response_code(200);
    ret.set_message("OK");
    ret.AppendToBody(file_content);
  }
  return ret;
}

static HttpResponse ProcessQueryRequest(const string &uri,
                                 WordIndex *index) {
  // The response we're building up.
  HttpResponse ret;

  // Your job here is to figure out how to present the user with
  // the same query interface as our solution_binaries/httpd server.
  // A couple of notes:
  //
  //  - no matter what, you need to present the 595gle logo and the
  //    search box/button
  //
  //  - if the user sent in a search query, you also
  //    need to display the search results. You can run the solution binaries to see how these should look
  //
  //  - you'll want to use the URLParser to parse the uri and extract
  //    search terms from a typed-in search query.  convert them
  //    to lower case.
  //
  //  - Use the specified index to generate the query results

  ret.set_protocol("HTTP/1.1");
  ret.set_response_code(200);
  ret.set_message("OK");
  string summary = kFivegleStr;
  
  URLParser parser;
  parser.parse(uri);
  std::map<string, string> args = parser.args();

  // first entry
  if (args.find("terms") == args.end()) {
    summary += "</body>\n</html>\n";
    ret.AppendToBody(summary);
    return ret;
  }
  
  // query
  string terms = args["terms"];
  vector<string> search_terms;
  boost::split(search_terms, terms, boost::is_any_of(" "));
  string keywords = boost::join(search_terms, " ");
  list<Result> results = index->lookup_query(search_terms);

  if (results.size() == 0) {
    summary += "<p><br>\nNo results found for <b>" + keywords + "</b>\n<p>\n\n</body>\n</html>\n";
  } else {
    summary += "<p><br>\n";
    summary += std::to_string(results.size()) + " results found for <b>" + escape_html(keywords) + "</b>\n</p>\n\n<ul>\n";
    for (Result res : results) {
      summary += " <li> <a href=\"/static/" + res.doc_name + "\">" + res.doc_name + "</a> [" + std::to_string(res.rank) + "]<br>\n";
    }
    summary += "</ul>\n</body>\n</html>\n";
  }
  ret.AppendToBody(summary);
  return ret;
}

}  // namespace searchserver

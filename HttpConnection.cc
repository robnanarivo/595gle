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

#include <cstdint>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <map>
#include <string>
#include <vector>

#include "./HttpRequest.h"
#include "./HttpUtils.h"
#include "./HttpConnection.h"

using std::map;
using std::string;
using std::vector;

namespace searchserver {

static const char *kHeaderEnd = "\r\n\r\n";
static const int kHeaderEndLen = 4;

bool HttpConnection::next_request(HttpRequest *request) {
  // Use "wrapped_read" to read data into the buffer_
  // instance variable.  Keep reading data until either the
  // connection drops or you see a "\r\n\r\n" that demarcates
  // the end of the request header.
  //
  // Once you've seen the request header, use parse_request()
  // to parse the header into the *request argument.
  //
  // Very tricky part:  clients can send back-to-back requests
  // on the same socket.  So, you need to preserve everything
  // after the "\r\n\r\n" in buffer_ for the next time the
  // caller invokes next_request()!

  size_t next;
  while (true) {
    next = buffer_.find(kHeaderEnd, 0, kHeaderEndLen);
    if (next != string::npos) {
      break;
    }
    wrapped_read(fd_, &buffer_);
  }

  // set request and new buffer_
  string request_str = buffer_.substr(0, next + kHeaderEndLen);
  buffer_ = buffer_.substr(next + kHeaderEndLen, string::npos);

  // parse the request
  return parse_request(request_str, request);
}

bool HttpConnection::write_response(const HttpResponse &response) {
  // Implement so that the response is converted to a string
  // and written out to the socket for this connection

  const string response_str = response.GenerateResponseString();
  int res = wrapped_write(fd_, response_str);
  if (res < 0) {
    return false;
  }
  return true;
}

bool HttpConnection::parse_request(const string &request, HttpRequest* out) {
  HttpRequest req("/");  // by default, get "/".

  // Split the request into lines.  Extract the URI from the first line
  // and store it in req.URI.  For each additional line beyond the
  // first, extract out the header name and value and store them in
  // req.headers_ (i.e., HttpRequest::AddHeader).  You should look
  // at HttpRequest.h for details about the HTTP header format that
  // you need to parse.
  //
  // You'll probably want to look up boost functions for (a) splitting
  // a string into lines on a "\r\n" delimiter, (b) trimming
  // whitespace from the end of a string, and (c) converting a string
  // to lowercase.
  //
  // If a request is malfrormed, return false, otherwise true and 
  // the parsed request is retrned via *out
  
  vector<string> lines;
  boost::split(lines, request, boost::is_any_of("\r\n"));
  if (lines.size() < 1) {
    return false;
  }

  // uri
  vector<string> uri_split;
  boost::split(uri_split, lines[0], boost::is_any_of(" "));
  if (uri_split.size() < 3) {
    return false;
  }
  req.set_uri(uri_split[1]);;  

  // headers
  for (size_t i = 1; i < lines.size(); i++) {
    if (lines[i].empty()) {
      continue;
    }
    vector<string> header_split;
    boost::split(header_split, lines[i], boost::is_any_of(":"));
    if (header_split.size() < 2) {
      return false;
    }
    string header_name = header_split[0];
    string header_value = header_split[1];
    boost::trim(header_name);
    boost::trim(header_value);
    boost::to_lower(header_name);
    boost::to_lower(header_value);
    req.AddHeader(header_name, header_value);
  }

  *out = req;
  return true;
}

}  // namespace searchserver

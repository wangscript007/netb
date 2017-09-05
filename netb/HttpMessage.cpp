/*
 * Copyright (C) 2013, Maoxu Li. Email: maoxu@lebula.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "HttpMessage.hpp"
#include "StreamBuffer.hpp"
#include "StreamReader.hpp"
#include "StreamWriter.hpp"
#include <sstream>
#include <cassert>

NETB_BEGIN

const char* HttpMessage::CRLF = "\r\n";

HttpMessage::HttpMessage(const char* version) 
: _version(version)
, _body_len(0)
, _body(NULL)
, _state(STATE::READY)
{

}

HttpMessage::~HttpMessage()
{
    Reset();
}

void HttpMessage::Reset()
{
    for(std::vector<Header*>::iterator it = _headers.begin(); 
        it != _headers.end(); ++it)
    {
        Header* p = *it;
        delete p;
    }
    _headers.clear();

    if(!_body)
    {
        delete[] _body; 
        _body = NULL; 
    }
    _body_len = 0;

    _version = "HTTP/1.1";
    _state = STATE::READY;
}

const char* HttpMessage::GetVersion() const 
{
    return _version.c_str();
}

void HttpMessage::SetVersion(const char* version)
{
    _version = version;
}

void HttpMessage::SetHeader(const char* key, const char* value)
{
    std::vector<Header*>::iterator it(_headers.begin());
    while(it != _headers.end())
    {
        Header* p = *it;
        if(key == p->key)
        {
            p->value = value;
            return;
        }
        ++it;
    }
    _headers.push_back(new Header(key, value));
}

void HttpMessage::RemoveHeader(const char* key) 
{
    std::vector<Header*>::iterator it(_headers.begin());
    while(it != _headers.end())
    {
        Header* p = *it;
        if(key == p->key)
        {
            it = _headers.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

const char* HttpMessage::GetHeader(const char* key) const
{
    std::vector<Header*>::const_iterator it(_headers.begin());
    while(it != _headers.end())
    {
        Header* p = *it;
        if(p->key == key)
        {
            return p->value.c_str();
        }
        ++it;
    }
    return NULL;
}

long HttpMessage::GetHeaderAsInt(const char* key) const
{
    std::string value = GetHeader(key);
    std::istringstream iss(value);
    long v = 0;
    iss >> v;
    return v;
}

double HttpMessage::GetHeaderAsFloat(const char* key) const
{
    std::string value = GetHeader(key);
    std::istringstream iss(value);
    double v = 0;
    iss >> v;
    return v;
}

size_t HttpMessage::GetBodyLen() const
{
    return _body_len;
}

const void* HttpMessage::GetBody() const
{
    return _body;
}

// To string for debug
std::string HttpMessage::ToString() const 
{
    // <verb> SP <url> SP <protocol/version> CRLF
    // <headers> CRLF 
    // <buf>
    std::ostringstream oss;
    oss << StartLine() << "\r\n";
    for(std::vector<Header*>::const_iterator it = _headers.begin(); 
        it != _headers.end(); ++it)
    {
        Header* p = *it;
        oss << p->key << ":" << p->value << "\r\n";
    }
    oss << "\r\n";
    oss << "Body [" << _body_len << "]" << "\r\n";
    return oss.str();
}

// Todo: error handling
// How about if error occured during packaging?
// An incomplete packet may be in the buffer. 
bool HttpMessage::ToBuffer(StreamBuffer* buf) const
{
    StreamWriter writer(buf);
    if(!writer.String(StartLine(), CRLF)) return false;
    for(std::vector<Header*>::const_iterator it = _headers.begin();
        it != _headers.end(); ++it)
    {
        Header* p = *it;
        if(!writer.String(p->key, ':') || !writer.String(p->value, CRLF))
        {
            return false;
        }
    }
    writer.String(CRLF, 2);
    if(_body_len > 0 && _body != NULL)
    {
        if(!writer.Bytes(_body, _body_len))
        {
            return false;
        }
    }
    return true;
}

// Parse a HTTP message from a stream buffer
// This function will keep state until complete a messages
// Return true once a message is completed
bool HttpMessage::FromBuffer(StreamBuffer* buf)
{
    if(_state == STATE::READY) ReadStartLine(buf);
    if(_state == STATE::HEADER) ReadHeader(buf);
    if(_state == STATE::BODY) ReadBody(buf);
    return _state == STATE::OKAY;
}

// Read start line
// parsed by derived class
// Todo: detect error before reading
void HttpMessage::ReadStartLine(StreamBuffer* buf)
{
    assert(_state == STATE::READY);
    std::string line;
    if(StreamReader(buf).String(line, CRLF))
    {
        StartLine(line);
        _state = STATE::HEADER;
    }
}

// Read header lines
// Read each complete line, until a blank line
void HttpMessage::ReadHeader(StreamBuffer* buf)
{
    assert(_state == STATE::HEADER);
    std::string line;
    while(StreamReader(buf).String(line, CRLF))// Read all complete lines
    {       
        if(line.empty()) // Blank line, Separator line of headers and body, headers are completd
        {
            _body_len = GetHeaderAsInt("Content-Length");
            _state = _body_len > 0 ? STATE::BODY : STATE::OKAY;
            break;
        }
        // parse line to key and value
        size_t pos = line.find(":");
        assert(pos != std::string::npos);
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        SetHeader(key.c_str(), value.c_str());
    }
}

// Read http message body
void HttpMessage::ReadBody(StreamBuffer* buf)
{
    assert(_state == STATE::BODY);
    assert(_body_len > 0);
    if(_body == NULL) _body = new unsigned char[_body_len];
    if(StreamReader(buf).Bytes(_body, _body_len))
    {
        _state = STATE::OKAY;
    }
}

//////////////////////////////////////////////////////////////////////////

HttpRequest::HttpRequest(const char* version) 
: HttpMessage(version)
, _method("GET")
{
   
}

HttpRequest::HttpRequest(const char* method, const char* url, const char* version)
: HttpMessage(version)
, _method(method)
, _url(url)
{

}

HttpRequest::~HttpRequest()
{

}

void HttpRequest::Reset() 
{
    HttpMessage::Reset();
    _method = "GET";
    _url = "";
}

const char* HttpRequest::GetMethod() const 
{
    return _method.c_str();
}

void HttpRequest::SetMethod(const char* method)
{
    _method = method;
}

const char* HttpRequest::GetUrl() const
{
    return _url.c_str();
}

void HttpRequest::SetUrl(const char* url)
{
    _url = url;
}

// Parse start line
// <verb> SP <url> SP <protocol/version> CRLF
bool HttpRequest::StartLine(const std::string& line)
{
    if(line.empty())
    {
        return false;
    }
    // Split sring with space
    std::istringstream iss(line);
    iss >> _method >> _url >> _version;
    return true;
}

// <verb> SP <url> SP <protocol/version> CRLF
std::string HttpRequest::StartLine() const
{
    std::ostringstream oss;
    oss << _method << " " << _url << " " << _version;
    return oss.str();
}

//////////////////////////////////////////////////////////////////////////////////

HttpResponse::HttpResponse(const char* version) 
: HttpMessage(version) 
, _code(200)
{

}

HttpResponse::~HttpResponse()
{

}

void HttpResponse::Reset() 
{
    HttpMessage::Reset();
    _code = 200;
    _phrase = "OK";
}

void HttpResponse::SetStatus(int code, const char* phrase)
{
    _code = code;
    _phrase = phrase;
}

int HttpResponse::GetCode() const
{
    return _code;
}

const char* HttpResponse::GetPhrase() const
{
    return (_phrase.empty() ? s_default_phrases[_code] : _phrase.c_str());
}


// HTTP response status code and default phrase
std::map<int, const char*> HttpResponse::s_default_phrases = 
{
    { 100, "Continue" },

    { 200, "OK" },
    { 201, "Created" },
    { 250, "Low on Storage Space" },

    { 300, "Multiple Choices" },
    { 301, "Moved Permanently" },
    { 302, "Moved Temporarily" },
    { 303, "See Other" },
    { 304, "Not Modified" },
    { 305, "Use Proxy" },

    { 400, "Bad Request" },
    { 401, "Unauthorized" },
    { 402, "Payment Required" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
    { 405, "Method Not Allowed" },
    { 406, "Not Acceptable" },
    { 407, "Proxy Authentication Required" },
    { 408, "Request Time-out" },
    { 410, "Gone" },
    { 411, "Length Required" },
    { 412, "Precondition Failed" },
    { 413, "Request Entity Too Large" },
    { 414, "Request-URI Too Large" },
    { 415, "Unsupported Media Type" },
    { 451, "Parameter Not Understood" },
    { 452, "Conference Not Found" },
    { 453, "Not Enough Bandwidth" },
    { 454, "Session Not Found" },
    { 455, "Method Not Valid in This State" },
    { 456, "Header Field Not Valid for Resource" },
    { 457, "Invalid Range" },
    { 458, "Parameter Is Read-Only" },
    { 459, "Aggregate operation not allowed" },
    { 460, "Only aggregate operation allowed" },
    { 461, "Unsupported transport" },
    { 462, "Destination unreachable" },

    { 500, "Internal Server Error" },
    { 501, "Not Implemented" },
    { 502, "Bad Gateway" },
    { 503, "Service Unavailable" },
    { 504, "Gateway Time-out" },
    { 505, "Rtsp Version not supported" },
    { 551, "Option not supported" },
    {   0, NULL }
};

// HTTP/1.1 code phrase CRLF
bool HttpResponse::StartLine(const std::string& line)
{
    if(line.empty())
    {
        return false;
    }
    std::istringstream iss(line);
    iss >> _version >> _code >> _phrase;
    return true;
}

std::string HttpResponse::StartLine() const
{
    // <protocol>/<version> code phrase CRLF
    std::ostringstream oss;
    oss << _version << " " << _code << " " 
        << (_phrase.empty() ? s_default_phrases[_code] : _phrase);
    return oss.str();
}

NETB_END
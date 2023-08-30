/*
 * @Author       : mark
 * @Date         : 2020-06-26
 * @copyleft Apache 2.0
 */
#include "httprequest.h"
#include <algorithm>
#include <mysql/mysql_com.h>
#include <regex>
using namespace std;

const unordered_set<string> HttpRequest::DEFAULT_HTML{
    "/index",
    "/register",
    "/login",
    "/welcome",
    "/video",
    "/picture",
};

const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG{
    {"/register.html", 0},
    {"/login.html", 1},
};

void HttpRequest::Init() {
    method_ = path_ = version_ = body_ = "";
    state_ = PARSE_STATE::REQUEST_LINE;
    header_.clear();
    post_.clear();
}

bool HttpRequest::IsKeepAlive() const {
    if (header_.count("Connection") == 1) {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}

bool HttpRequest::parse(Buffer &buff) {
    const char CRLF[] = "\r\n";
    if (buff.ReadableBytes() < 0) {
        return false;
    }
    while (buff.ReadableBytes() > 0 && state_ != PARSE_STATE::FINISH) {
        const char *lineEnd = std::search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
        // 创建一个迭代器
        std::string line(buff.Peek(), lineEnd);
        switch (state_) {
        case PARSE_STATE::REQUEST_LINE:
            if (!ParseRequestLine_(line)) {
                return false;
            }
            ParsePath_();
            break;
        case PARSE_STATE::HEADERS:
            ParseHeader_(line);
            if (buff.ReadableBytes() <= 2) { // 空行
                state_ = PARSE_STATE::FINISH;
            }
            break;
        case PARSE_STATE::BODY:
            ParseBody_(line);
        default: break;
        }
        if (lineEnd == buff.BeginWrite()) { break; }
        buff.RetrieveUntil(lineEnd + 2);
    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}

void HttpRequest::ParsePath_() {
    if (path_ == "/") {
        path_ = "/index.html";
    } else {
        for (auto &item : HttpRequest::DEFAULT_HTML) {
            path_ += ".html";
            break;
        }
    }
}

bool HttpRequest::ParseRequestLine_(const std::string &line) {
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    smatch submatch;
    if (regex_match(line, submatch, patten)) {
        method_ = submatch[1];
        path_ = submatch[2];
        version_ = submatch[3];
        state_ = PARSE_STATE::HEADERS;
        return true;
    }
    LOG_ERROR("RequestLine error");
    return false;
}

void HttpRequest::ParseHeader_(const std::string &line) {
    regex patten("^([^:]*) ?(.*)$");
    smatch submatch;
    if (regex_match(line, submatch, patten)) {
        header_[submatch[1]] = submatch[2];
    } else {
        state_ = PARSE_STATE::BODY;
    }
}

void HttpRequest::ParseBody_(const std::string &line) {
    body_ = line; // 空行
    ParsePost_();
    state_ = PARSE_STATE::FINISH;
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

int HttpRequest::ConverHex(char ch) {
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return ch;
}

void HttpRequest::ParsePost_() {
    if (method_ == "Post" && header_["Context-type"] == "application/x-www-form-urlencoded") {
        ParseFromUrlencoded_();
        if (DEFAULT_HTML_TAG.count(path_)) {
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("Tag: %d", tag);
            if (tag == 0 || tag == 1) {
                bool isLogin = (tag == 1);
                if (UserVerify(post_["username"], post_["password"], isLogin)) {
                    path_ = "/welcome.html";
                } else {
                    path_ = "/error.html";
                }
            }
        }
    }
}

bool HttpRequest::UserVerify(const std::string &name, const std::string &pwd, bool isLogin) {
    if (name == "" || pwd == "") { return false; }
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    MYSQL *sql;
    SqlConnRAII(&sql, SqlConnPool::Instance());
    assert(sql);

    bool flag = false;
    unsigned int j = 0;
    char order[256] = {0};
    MYSQL_FIELD *fields = nullptr;
    MYSQL_RES *res = nullptr;

    if (!isLogin) { flag = true; }
    /* 查询用户及密码 */
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);

    if (mysql_query(sql, order)) {
        mysql_free_result(res);
        return false;
    }
    res = mysql_store_result(sql);
    j = mysql_num_fields(res);
    fields = mysql_fetch_fields(res);

    while (MYSQL_ROW row = mysql_fetch_row(res)) {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        string password(row[1]);
        /* 注册行为 且 用户名未被使用*/
        if (isLogin) {
            if (pwd == password) {
                flag = true;
            } else {
                flag = false;
                LOG_DEBUG("pwd error!");
            }
        } else {
            flag = false;
            LOG_DEBUG("user used!");
        }
    }
    mysql_free_result(res);

    /* 注册行为 且 用户名未被使用*/
    if (!isLogin && flag == true) {
        LOG_DEBUG("regirster!");
        bzero(order, 256);
        snprintf(order, 256, "INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG("%s", order);
        if (mysql_query(sql, order)) {
            LOG_DEBUG("Insert error!");
            flag = false;
        }
        flag = true;
    }
    SqlConnPool::Instance()->FreeConn(sql);
    LOG_DEBUG("UserVerify success!!");
    return flag;
}

void HttpRequest::ParseFromUrlencoded_() {
    if (body_.size() == 0) {
        return;
    }
    std::string key, value;
    int n = body_.size();
    int i = 0, j = 0;
    int num = 0;
    for (; i < n; i++) {
        char ch = body_[i];
        switch (ch) {
        case '=':
            key = body_.substr(j, i - j);
            j = i + 1;
            break;
        case '+':
            body_[i] = ' ';
            break;
        case '%':
            num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
            body_[i + 2] = num % 10 + '0';
            body_[i + 1] = num / 10 + '0';
            i += 2;
            break;
        case '&':
            value = body_.substr(j, i - j);
            j = i + 1;
            post_[key] = value;
            LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
            break;
        default:
            break;
        }
    }
    assert(j <= i);
    // 只有一个key value
    if (post_.count(key) == 0 && j < i) {
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}

std::string HttpRequest::path() const {
    return path_;
}

std::string &HttpRequest::path() {
    return path_;
}
std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}

std::string HttpRequest::GetPost(const std::string &key) const {
    assert(key != "");
    if (post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char *key) const {
    assert(key != nullptr);
    if (post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}
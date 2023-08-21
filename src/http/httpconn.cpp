#include "httpconn.h"
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <bits/types/struct_iovec.h>
#include <cstring>
#include <fcntl.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>

const char *OK_200_TITLE = "OK";
const char *ERROR_400_TITLE = "Bad Request";
const char *ERROR_400_FORM =
    "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char *ERROR_403_TITLE = "Forbidden";
const char *ERROR_403_FORM =
    "You do not have permission to get file form this server.\n";
const char *ERROR_404_TITLE = "Not Found";
const char *ERROR_404_FORM =
    "The requested file was not found on this server.\n";
const char *ERROR_500_TITLE = "Internal Error";
const char *ERROR_500_FORM =
    "There was an unusual problem serving the request file.\n";

Epoll *HttpConn::epollPtr = nullptr;
char *HttpConn::resPath = nullptr;
int HttpConn::userCount = 0;
bool HttpConn::isCloseLog = true;
bool HttpConn::isET = false;

void HttpConn::Init_() {
  readIdx_ = 0;
  checkIdx_ = 0;
  startLine_ = 0;
  writeIdx_ = 0;
  checkState_ = CHECK_STATE::REQUESTLINE;
  memset(readBuff_, '\0', READ_BUFF_SIZE);
  memset(writeBuff_, '\0', WRITE_BUFF_SIZE);
  requestMsg_ = {METHOD::GET, 0,       nullptr, nullptr,
                 nullptr,     nullptr, false,   false};
  bytesTosSend_ = 0;
  bytesHaveSend_ = 0;
}
void HttpConn::CloseConn() {
  epollPtr->RemoveFd(fd_);
  userCount--;
  fd_ = -1;
}

bool HttpConn::read() {
  if (readIdx_ > READ_BUFF_SIZE) {
    return false;
  }
  int len = 0;
  if (!isET) { // LT
    len = recv(fd_, readBuff_ + readIdx_, READ_BUFF_SIZE - readIdx_, 0);
    readIdx_ += len;
    if (len <= 0) {
      return false;
    }
    return true;
  } else { // ET 一次必须全部读完
    while (true) {
      len = recv(fd_, readBuff_ + readIdx_, READ_BUFF_SIZE - readIdx_, 0);
      if (len == -1) {
        // 读完就退出
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          break;
        }
        return false;
      } else if (len == 0) {
        return false;
      }
      readIdx_ += len;
    }
    return true;
  }
}

bool HttpConn::write() {
  if (bytesTosSend_ == 0) {
    epollPtr->Modify(fd_, EPOLLIN, isET);
    Init_();
    return true;
  }
  size_t offset = 0;
  while (true) {
    int len = writev(fd_, iov_, iovCount_);
    if (len > 0) {
      bytesHaveSend_ += len;
      offset = bytesHaveSend_ - len;
    } else {
      if (errno == EAGAIN) {                     // 判断缓冲区是否满了
        if (bytesHaveSend_ >= iov_[0].iov_len) { // 请求头发送完了
          iov_[0].iov_len = 0;
          iov_[1].iov_base = fileAddr_ + offset; // 存在文件偏移offset的位置
          iov_[1].iov_len = bytesTosSend_;
        } else { //继续发送请求头数据
          iov_[0].iov_base = writeBuff_ + bytesTosSend_;
          iov_[0].iov_len = bytesHaveSend_;
        }
        epollPtr->Modify(fd_, EPOLLOUT, isET);
        return true;
      }
      // 不是缓冲区满了，则是出错了，取消映射
      Unmap_();
      return false;
    }
    bytesTosSend_ -= len;
    if (bytesTosSend_ <= 0) {
      Unmap_();
      epollPtr->Modify(fd_, EPOLLIN, isET);
      if (requestMsg_.isKeepAlive) {
        Init_();
        return true;
      } else {
        return false;
      }
    }
  }
}

void HttpConn::Unmap_() {
  if (fileAddr_) {
    munmap(fileAddr_, fileStat_.st_size);
    fileAddr_ = nullptr;
  }
}

HttpConn::LINE_STATUS HttpConn::ParseLine_() {
  for (; checkIdx_ < readIdx_; checkIdx_++) {
    char ch = readBuff_[checkIdx_];
    if (ch == '\r') {
      if (checkIdx_ + 1 == readIdx_) { // 接受不完整
        return LINE_STATUS::LINE_OPEN;
      } else if (readBuff_[checkIdx_ + 1] == '\n') { //接受完整
        readBuff_[checkIdx_++] = '\0';
        readBuff_[checkIdx_++] = '\0';
        return LINE_STATUS::LINE_OK;
      }
      return LINE_STATUS::LINE_BAD;
    } else if (ch == '\n') {
      if (checkIdx_ > 1 && readBuff_[checkIdx_ - 1] == '\r') { // 上次接收不完整
        readBuff_[checkIdx_++] = '\0';
        readBuff_[checkIdx_++] = '\0';
        return LINE_STATUS::LINE_OK;
      }
      return LINE_STATUS::LINE_BAD;
    }
  }
  return LINE_STATUS::LINE_OPEN;
}

HttpConn::HTTP_CODE HttpConn::ParseRequestLine_(char *text) {
  // 解析 method http vision
  char *url = strpbrk(text, " \t"); // 匹配 空格 和 \t中的一个
  if (!url) {
    return HTTP_CODE::BAD_REQUEST;
  }
  *url++ = '\0';
  char *method = text;
  if (strcasecmp(method, "GET") == 0) {
    requestMsg_.method = METHOD::GET;
  } else if (strcasecmp(method, "POST") == 0) {
    requestMsg_.method = METHOD::POST;
    requestMsg_.cgi = true;
  } else {
    return HTTP_CODE::BAD_REQUEST;
  }
  // 跳过多于的空格 /t
  url += strspn(url, " \t");
  requestMsg_.version = strpbrk(url, " \t");
  if (!requestMsg_.version) {
    return HTTP_CODE::BAD_REQUEST;
  }
  *requestMsg_.version++ = '\0';
  requestMsg_.version =
      strpbrk(requestMsg_.version, " \t"); // 去除多余的空格 \t
  if (strcasecmp(requestMsg_.version, "HTTP/1.1") != 0) {
    return HTTP_CODE::BAD_REQUEST;
  }
  //跳过请求中的http://.../
  if (strncasecmp(url, "http://", 7) == 0) {
    url += 7;
    requestMsg_.url = strchr(url, '/');
  } else if (strncasecmp(url, "https://", 8) == 0) {
    url += 8;
    requestMsg_.url = strchr(url, '/');
  }

  if (!url || url[0] != '/') {
    return HTTP_CODE::BAD_REQUEST;
  }
  if (strlen(url) == 1) {
    strcat(requestMsg_.url, "judge.html");
  }
  checkState_ = CHECK_STATE::HEADER;
  return HTTP_CODE::NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::ParseHeader_(char *text) {
  if (text[0] == '\0') {
    if (requestMsg_.contextLen > 0) {
      checkState_ = CHECK_STATE::CONTENT;
    }
  } else if (strncasecmp(text, "Connection:", 11) == 0) {
    text += 11;
    text += strspn(text, " \t");
    if (strcasecmp(text, "keep-alive") == 0) {
      requestMsg_.isKeepAlive = true;
    }
  } else if (strncasecmp(text, "Context-length:", 15) == 0) {
    text += 15;
    text += strspn(text, " \t");
    requestMsg_.contextLen = atoi(text);
  } else if (strncasecmp(text, "Host", 5) == 0) {
    text += 5;
    text += strspn(text, " \t");
    requestMsg_.host = text;
  } else {
    LOG_INFO("unknow header: %s", text);
  }
  return HTTP_CODE::NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::ParseContent_(char *text) {
  if (readIdx_ >= checkIdx_ + requestMsg_.contextLen) { // GET请求
    text[requestMsg_.contextLen] = '\0';
    requestMsg_.context = text;
    return HTTP_CODE::GET_REQUEST;
  }
  return HTTP_CODE::NO_REQUEST;
}

char *HttpConn::GetLine_() { return readBuff_ + startLine_; }

HttpConn::HTTP_CODE HttpConn::ProcessRead_() {
  LINE_STATUS lineStatue = LINE_STATUS::LINE_OK;
  HTTP_CODE ret = HTTP_CODE::NO_REQUEST;
  char *text;
  while ((lineStatue = ParseLine_()) == LINE_STATUS::LINE_OK ||
         (lineStatue == LINE_STATUS::LINE_OK &&
          checkState_ == CHECK_STATE::CONTENT)) {
    text = GetLine_();
    startLine_ = checkIdx_;
    switch (checkState_) {
    case CHECK_STATE::REQUESTLINE:
      ret = ParseRequestLine_(text);
      if (ret == HTTP_CODE::BAD_REQUEST) {
        return ret;
      }
      break;
    case CHECK_STATE::HEADER:
      ret = ParseHeader_(text);
      if (ret == HTTP_CODE::BAD_REQUEST) {
        return ret;
      }
      break;
    case CHECK_STATE::CONTENT:
      ret = ParseContent_(text);
      lineStatue = LINE_STATUS::LINE_OPEN;
      break;
    default:
      return HTTP_CODE::INTERNAL_ERROR;
      break;
    }
  }
  if (ret == HTTP_CODE::GET_REQUEST) { // GET 请求
    return DoRequest_();
  }
  return ret;
}
// 生成相应报文
HttpConn::HTTP_CODE HttpConn::DoRequest_() {
  char filePath[PATH_LEN];
  strcpy(filePath, resPath);
  int len = strlen(resPath);
  const char *p = strrchr(requestMsg_.url, '/');
  char fileName[PATH_LEN];
  switch (p[1]) {
  case '0': {
    strcpy(fileName, "/register.html");
    break;
  }
  case '1': {
    strcpy(fileName, "/log.html");
    break;
  }
  default:
    strcpy(fileName, requestMsg_.url);
    break;
  }
  strncpy(filePath + len, fileName, PATH_LEN - len - 1);
  if (stat(fileName, &fileStat_) < 0) {
    return HTTP_CODE::NO_RESOURSE;
  }
  //  禁止其他人读
  if (!(fileStat_.st_mode & S_IROTH)) {
    return HTTP_CODE::FORBIDDENT_REQUEST;
  }
  // 要读取的文件是一个目录
  if (S_ISDIR(fileStat_.st_mode)) {
    return HTTP_CODE::BAD_REQUEST;
  }
  int fd = open(filePath, O_RDONLY);
  fileAddr_ = (char *)mmap(0, fileStat_.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  return HTTP_CODE::FILE_REQUEST;
}

bool HttpConn::AddResponse_(const char *format, ...) {
  if (writeIdx_ >= WRITE_BUFF_SIZE) {
    return false;
  }
  va_list args;
  va_start(args, format);
  size_t len = vsnprintf(writeBuff_ + writeIdx_,
                         WRITE_BUFF_SIZE - 1 - writeIdx_, format, args);
  if (len >= (WRITE_BUFF_SIZE - 1 - writeIdx_)) {
    va_end(args);
    return false;
  }
  writeIdx_ += len;
  va_end(args);
  // LOG_INFO("request:%s", writeBuff_);
  return true;
}

// 
bool HttpConn::AddContextType_() {
  return AddResponse_("Context-Type:%s\r\n", "text/html");
}
// 添加链接状态
bool HttpConn::AddLinger_() {
  return AddResponse_("Connection:%s\r\n", (requestMsg_.isKeepAlive == true)
                                               ? "keep-alive"
                                               : "close");
}
// 添加空行
bool HttpConn::AddBlinkLine_() { return AddResponse_("%s", "\r\n"); }

bool HttpConn::AddStatusLine_(int status, const char *title) {
  return AddResponse_("%d %d %s\r\n", "HTTP/1.1", status, title);
}
// 添加文本长度
bool HttpConn::AddContentLength_(int len) {
  return AddResponse_("Content-Length:%d\r\n", len);
}

// 添加消息报头，具体的添加文本长度、连接状态和空行
bool HttpConn::AddHeader_(int len) {
  return AddContentLength_(len) && AddLinger_() && AddBlinkLine_();
}
// 添加文本
bool HttpConn::AddContent_(const char *content) {
  return AddResponse_("%s", content);
}

bool HttpConn::ProcessWrite_(HTTP_CODE ret) {
  bool flag;
  switch (ret) {
    // 内部错误
  case HTTP_CODE::INTERNAL_ERROR: {
    flag = (AddStatusLine_(500, ERROR_500_TITLE) &&
            AddHeader_(strlen(ERROR_500_FORM)) && AddContent_(ERROR_500_FORM));
    if (!flag) {
      return false;
    }
  } break;
  case HTTP_CODE::BAD_REQUEST: {
    flag =
        (AddStatusLine_(404, ERROR_404_TITLE) &&
         AddHeader_(strlen(ERROR_404_TITLE)) && AddContent_(ERROR_404_TITLE));
    if (!flag) {
      return false;
    }
  } break;
  case HTTP_CODE::FORBIDDENT_REQUEST: {
    flag =
        (AddStatusLine_(403, ERROR_403_TITLE) &&
         AddHeader_(strlen(ERROR_403_TITLE)) && AddContent_(ERROR_403_TITLE));
    if (!flag) {
      return false;
    }
  } break;
  case HTTP_CODE::FILE_REQUEST: {
    if (fileStat_.st_size > 0) {
      AddHeader_(fileStat_.st_size);
      iov_[0].iov_base = writeBuff_;
      iov_[0].iov_len = writeIdx_;

      iov_[1].iov_base = fileAddr_;
      iov_[1].iov_len = fileStat_.st_size;
      iovCount_ = 2;

      bytesTosSend_ = writeIdx_ + fileStat_.st_size;
      return true;
    } else {
    }
  }
  default:
    return false;
    break;
  }

  iov_[0].iov_base = writeBuff_;
  iov_[0].iov_len = writeIdx_;
  iovCount_ = 1;
  return true;
}

void HttpConn::process() {
  HTTP_CODE readRet = ProcessRead_();
  //请求不完整
  if (readRet == HTTP_CODE::NO_REQUEST) {
    epollPtr->Modify(fd_, EPOLLIN, isET);
    return;
  }

  bool writeRet = ProcessWrite_(readRet);
  if (!writeRet)
    CloseConn();
  epollPtr->Modify(fd_, EPOLLOUT, isET);
}
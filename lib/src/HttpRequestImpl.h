/**
 *
 *  HttpRequestImpl.h
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

#include "HttpUtils.h"
#include "CacheFile.h"
#include <drogon/utils/Utilities.h>
#include <drogon/HttpRequest.h>
#include <drogon/utils/Utilities.h>
#include <trantor/net/EventLoop.h>
#include <trantor/net/InetAddress.h>
#include <trantor/utils/Logger.h>
#include <trantor/utils/MsgBuffer.h>
#include <trantor/utils/NonCopyable.h>
#include <algorithm>
#include <string>
#include <thread>
#include <unordered_map>
#include <assert.h>
#include <stdio.h>

namespace drogon
{
class HttpRequestImpl : public HttpRequest
{
  public:
    friend class HttpRequestParser;

    explicit HttpRequestImpl(trantor::EventLoop *loop)
        : creationDate_(trantor::Date::now()), loop_(loop)
    {
    }
    void reset()
    {
        LOG_DEBUG << "dude!!!";
        method_ = Invalid;
        version_ = Version::kUnknown;
        contentLen_ = 0;
        flagForParsingJson_ = false;
        headers_.clear();
        cookies_.clear();
        flagForParsingParameters_ = false;
        path_.clear();
        matchedPathPattern_ = "";
        query_.clear();
        parameters_.clear();
        jsonPtr_.reset();
        sessionPtr_.reset();
        attributesPtr_.reset();
        cacheFilePtr_.reset();
        expect_.clear();
        content_.clear();
        contentType_ = CT_TEXT_PLAIN;
        contentTypeString_.clear();
        keepAlive_ = true;
    }
    trantor::EventLoop *getLoop()
    {
        return loop_;
    }

    void setVersion(Version v)
    {
        version_ = v;
        if (version_ == Version::kHttp10)
        {
            keepAlive_ = false;
        }
    }

    virtual Version version() const override
    {
        return version_;
    }

    bool setMethod(const char *start, const char *end);
    void setSecure(bool secure)
    {
        isOnSecureConnection_ = secure;
    }

    virtual void setMethod(const HttpMethod method) override
    {
        method_ = method;
        return;
    }

    virtual HttpMethod method() const override
    {
        return method_;
    }

    virtual const char *methodString() const override;

    void setPath(const char *start, const char *end)
    {
        if (utils::needUrlDecoding(start, end))
        {
            path_ = utils::urlDecode(start, end);
        }
        else
        {
            path_.append(start, end);
        }
    }

    virtual void setPath(const std::string &path) override
    {
        path_ = path;
    }

    virtual const std::unordered_map<std::string, std::string> &parameters()
        const override
    {
        parseParametersOnce();
        return parameters_;
    }

    virtual const std::string &getParameter(
        const std::string &key) const override
    {
        const static std::string defaultVal;
        parseParametersOnce();
        auto iter = parameters_.find(key);
        if (iter != parameters_.end())
            return iter->second;
        return defaultVal;
    }

    virtual const std::string &path() const override
    {
        return path_;
    }

    void setQuery(const char *start, const char *end)
    {
        query_.assign(start, end);
    }

    void setQuery(const std::string &query)
    {
        query_ = query;
    }

    string_view bodyView() const
    {
        if (cacheFilePtr_)
        {
            return cacheFilePtr_->getStringView();
        }
        return content_;
    }
    virtual const char *bodyData() const override
    {
        if (cacheFilePtr_)
        {
            return cacheFilePtr_->getStringView().data();
        }
        return content_.data();
    }
    virtual size_t bodyLength() const override
    {
        if (cacheFilePtr_)
        {
            return cacheFilePtr_->getStringView().length();
        }
        return content_.length();
    }

    void appendToBody(const char *data, size_t length)
    {
        if (cacheFilePtr_)
        {
            cacheFilePtr_->append(data, length);
        }
        else
        {
            content_.append(data, length);
        }
    }

    void reserveBodySize();

    string_view queryView() const
    {
        return query_;
    }

    string_view contentView() const
    {
        if (cacheFilePtr_)
            return cacheFilePtr_->getStringView();
        return content_;
    }

    virtual const std::string &query() const override
    {
        return query_;
    }

    virtual const trantor::InetAddress &peerAddr() const override
    {
        return peer_;
    }

    virtual const trantor::InetAddress &localAddr() const override
    {
        return local_;
    }

    virtual const trantor::Date &creationDate() const override
    {
        return creationDate_;
    }

    void setCreationDate(const trantor::Date &date)
    {
        creationDate_ = date;
    }

    void setPeerAddr(const trantor::InetAddress &peer)
    {
        peer_ = peer;
    }

    void setLocalAddr(const trantor::InetAddress &local)
    {
        local_ = local;
    }

    void addHeader(const char *start, const char *colon, const char *end);

    const std::string &getHeader(const std::string &field) const override
    {
        auto lowField = field;
        std::transform(lowField.begin(),
                       lowField.end(),
                       lowField.begin(),
                       tolower);
        return getHeaderBy(lowField);
    }

    const std::string &getHeader(std::string &&field) const override
    {
        std::transform(field.begin(), field.end(), field.begin(), tolower);
        return getHeaderBy(field);
    }

    const std::string &getHeaderBy(const std::string &lowerField) const
    {
        const static std::string defaultVal;
        auto it = headers_.find(lowerField);
        if (it != headers_.end())
        {
            return it->second;
        }
        return defaultVal;
    }

    const std::string &getCookie(const std::string &field) const override
    {
        const static std::string defaultVal;
        auto it = cookies_.find(field);
        if (it != cookies_.end())
        {
            return it->second;
        }
        return defaultVal;
    }

    const std::unordered_map<std::string, std::string> &headers() const override
    {
        return headers_;
    }

    const std::unordered_map<std::string, std::string> &cookies() const override
    {
        return cookies_;
    }

    virtual void setParameter(const std::string &key,
                              const std::string &value) override
    {
        flagForParsingParameters_ = true;
        parameters_[key] = value;
    }

    const std::string &getContent() const
    {
        return content_;
    }

    void swap(HttpRequestImpl &that) noexcept;

    void setContent(const std::string &content)
    {
        content_ = content;
    }

    virtual void setBody(const std::string &body) override
    {
        content_ = body;
    }

    virtual void setBody(std::string &&body) override
    {
        content_ = std::move(body);
    }

    virtual void addHeader(const std::string &key,
                           const std::string &value) override
    {
        headers_[key] = value;
    }

    virtual void addCookie(const std::string &key,
                           const std::string &value) override
    {
        cookies_[key] = value;
    }

    virtual void setPassThrough(bool flag) override
    {
        passThrough_ = flag;
    }

    bool passThrough() const
    {
        return passThrough_;
    }

    void appendToBuffer(trantor::MsgBuffer *output) const;

    virtual SessionPtr session() const override
    {
        return sessionPtr_;
    }

    void setSession(const SessionPtr &session)
    {
        sessionPtr_ = session;
    }

    virtual AttributesPtr attributes() const override
    {
        if (!attributesPtr_)
        {
            attributesPtr_ = std::make_shared<Attributes>();
        }
        return attributesPtr_;
    }

    virtual const std::shared_ptr<Json::Value> jsonObject() const override
    {
        // Not multi-thread safe but good, because we basically call this
        // function in a single thread
        if (!flagForParsingJson_)
        {
            flagForParsingJson_ = true;
            parseJson();
        }
        return jsonPtr_;
    }

    virtual void setCustomContentTypeString(const std::string &type) override
    {
        contentType_ = CT_NONE;
        contentTypeString_ = type;
    }
    virtual void setContentTypeCode(const ContentType type) override
    {
        contentType_ = type;
        auto &typeStr = webContentTypeToString(type);
        setContentType(std::string(typeStr.data(), typeStr.length()));
    }

    // virtual void setContentTypeCodeAndCharacterSet(ContentType type, const
    // std::string &charSet = "utf-8") override
    // {
    //     contentType_ = type;
    //     setContentType(webContentTypeAndCharsetToString(type, charSet));
    // }

    virtual ContentType contentType() const override
    {
        return contentType_;
    }

    virtual const char *matchedPathPatternData() const override
    {
        return matchedPathPattern_.data();
    }
    virtual size_t matchedPathPatternLength() const override
    {
        return matchedPathPattern_.length();
    }

    void setMatchedPathPattern(const std::string &pathPattern)
    {
        matchedPathPattern_ = pathPattern;
    }
    const std::string &expect() const
    {
        return expect_;
    }
    bool keepAlive() const
    {
        return keepAlive_;
    }
    virtual bool isOnSecureConnection() const noexcept override
    {
        return isOnSecureConnection_;
    }

    ~HttpRequestImpl();

  protected:
    friend class HttpRequest;
    void setContentType(const std::string &contentType)
    {
        contentTypeString_ = contentType;
    }
    void setContentType(std::string &&contentType)
    {
        contentTypeString_ = std::move(contentType);
    }

  private:
    void parseParameters() const;
    void parseParametersOnce() const
    {
        // Not multi-thread safe but good, because we basically call this
        // function in a single thread
        if (!flagForParsingParameters_)
        {
            flagForParsingParameters_ = true;
            parseParameters();
        }
    }

    void parseJson() const;
    mutable bool flagForParsingParameters_{false};
    mutable bool flagForParsingJson_{false};
    HttpMethod method_{Invalid};
    Version version_{Version::kUnknown};
    std::string path_;
    string_view matchedPathPattern_{""};
    std::string query_;
    std::unordered_map<std::string, std::string> headers_;
    std::unordered_map<std::string, std::string> cookies_;
    mutable std::unordered_map<std::string, std::string> parameters_;
    mutable std::shared_ptr<Json::Value> jsonPtr_;
    SessionPtr sessionPtr_;
    mutable AttributesPtr attributesPtr_;
    trantor::InetAddress peer_;
    trantor::InetAddress local_;
    trantor::Date creationDate_;
    std::unique_ptr<CacheFile> cacheFilePtr_;
    std::string expect_;
    bool keepAlive_{true};
    bool isOnSecureConnection_{false};
    bool passThrough_{false};

  protected:
    std::string content_;
    size_t contentLen_{0};
    trantor::EventLoop *loop_;
    ContentType contentType_{CT_TEXT_PLAIN};
    std::string contentTypeString_;
};

using HttpRequestImplPtr = std::shared_ptr<HttpRequestImpl>;

inline void swap(HttpRequestImpl &one, HttpRequestImpl &two) noexcept
{
    one.swap(two);
}

}  // namespace drogon

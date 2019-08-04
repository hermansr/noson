/*
 *      Copyright (C) 2016 Jean-Luc Barriere
 *
 *  This library is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301 USA
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "securesocket.h"
#include "debug.h"
#include "cppdef.h"

#include <errno.h>

#ifdef __WINDOWS__
#include <WinSock2.h>
#define LASTERROR WSAGetLastError()
#define ERRNO_INTR WSAEINTR
#else
#define LASTERROR errno
#define ERRNO_INTR EINTR
#endif /* __WINDOWS__ */

using namespace NSROOT;

SSLSessionFactory* SSLSessionFactory::m_instance = 0;

SSLSessionFactory& SSLSessionFactory::Instance()
{
  if (!m_instance)
    m_instance = new SSLSessionFactory();
  return *m_instance;
}

void SSLSessionFactory::Destroy()
{
  SAFE_DELETE(m_instance);
}

#if HAVE_OPENSSL

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

/* Cipher suites, https://www.openssl.org/docs/apps/ciphers.html */
const char* const PREFERRED_CIPHERS = "HIGH:!aNULL:!kRSA:!SRP:!PSK:!CAMELLIA:!RC4:!MD5:!DSS";

SSLSessionFactory::SSLSessionFactory()
: m_enabled(false)
, m_ctx(NULL)
{
  if (SSL_library_init() < 0)
    DBG(DBG_ERROR, "%s: could not initialize the SSL library\n", __FUNCTION__);
  else
  {
    SSL_load_error_strings();
    /* SSL_load_error_strings loads both libssl and libcrypto strings */
    /* ERR_load_crypto_strings(); */

#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
    m_ctx = SSL_CTX_new(TLS_client_method());
#else
    m_ctx = SSL_CTX_new(SSLv23_client_method());
#endif
    if (m_ctx == NULL)
      DBG(DBG_ERROR, "%s: could not create the SSL context\n", __FUNCTION__);
    else
    {
      SSL_CTX_set_verify(static_cast<SSL_CTX*>(m_ctx), SSL_VERIFY_NONE, 0);

      /* Remove the most egregious. Because SSLv2 and SSLv3 have been removed,
       * a TLSv1.0 handshake is used. The client accepts TLSv1.0 and above.
       * An added benefit of TLS 1.0 and above are TLS extensions like Server
       * Name Indicatior (SNI).
       */
      const long flags = SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION;
      (void)SSL_CTX_set_options(static_cast<SSL_CTX*>(m_ctx), flags);

      /* Each cipher suite takes 2 bytes in the ClientHello, so advertising every
       * cipher suite available at the client is going to cause a big ClientHello
       * (or bigger then needed to get the job done).
       * When using SSL_CTX_set_cipher_list or SSL_set_cipher_list with the string
       * below you'll cut the number of cipher suites down to about 45.
       */
      if (SSL_CTX_set_cipher_list(static_cast<SSL_CTX*>(m_ctx), PREFERRED_CIPHERS) != 1)
        DBG(DBG_ERROR, "%s: Set cipher list failed\n", __FUNCTION__);

      /* The SSL trace callback is only used for verbose logging */
      /* SSL_CTX_set_msg_callback(static_cast<SSL_CTX*>(m_ctx), ssl_trace); */

      DBG(DBG_INFO, "%s: SSL engine initialized\n", __FUNCTION__);
      m_enabled = true;
    }
  }
}

SSLSessionFactory::~SSLSessionFactory()
{
  if (m_ctx)
    SSL_CTX_free(static_cast<SSL_CTX*>(m_ctx));
  ERR_free_strings();
  EVP_cleanup();
  DBG(DBG_INFO, "%s: SSL resources destroyed\n", __FUNCTION__);
}

SecureSocket* SSLSessionFactory::NewSocket()
{
  if (m_enabled)
  {
    SSL* ssl = SSL_new(static_cast<SSL_CTX*>(m_ctx));
    /* SSL_MODE_AUTO_RETRY
     * With this option set, if the server suddenly wants a new handshake,
     * OpenSSL handles it in the background. Without this option, any read
     * or write operation will return an error if the server wants a new
     * handshake, setting the retry flag in the process.
     */
    SSL_set_mode(static_cast<SSL*>(ssl), SSL_MODE_AUTO_RETRY);
    return new SecureSocket(ssl);
  }
  return NULL;
}

SecureSocket::SecureSocket(void* ssl)
: TcpSocket()
, m_ssl(ssl)
, m_cert(NULL)
, m_connected(false)
, m_ssl_error(0)
{
}

SecureSocket::~SecureSocket()
{
  Disconnect();
  SSL_free(static_cast<SSL*>(m_ssl));
}

bool SecureSocket::Connect(const char* server, unsigned port, int rcvbuf)
{
  m_ssl_error = 0;
  if (m_connected)
    Disconnect();

  /* Connect the tcp socket to the server */
  if (!TcpSocket::Connect(server, port, rcvbuf))
    return false;

  /* setup SSL */
  SSL_set_fd(static_cast<SSL*>(m_ssl), m_socket);
  SSL_set_tlsext_host_name(static_cast<SSL*>(m_ssl), server); /* fix SNI */

  /* do SSL handshake */
  for (;;)
  {
    int r = SSL_connect(static_cast<SSL*>(m_ssl));
    if (r > 0)
      break;
    if (r < 0)
    {
      int err = SSL_get_error(static_cast<SSL*>(m_ssl), r);
      if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ)
      {
        DBG(DBG_DEBUG, "%s: SSL retry (%d)\n", __FUNCTION__, err);
        continue;
      }
    }
    const char* errmsg = ERR_error_string(ERR_get_error(), NULL);
    DBG(DBG_ERROR, "%s: SSL connect failed: %s\n", __FUNCTION__, errmsg);
    TcpSocket::Disconnect();
    return false;
  }
  DBG(DBG_PROTO, "%s: SSL handshake initialized\n", __FUNCTION__);
  m_connected = true;
  /* check for a valid certificate */
  std::string str("");
  if (!IsCertificateValid(str))
  {
    DBG(DBG_ERROR, "%s: could not get a valid certificate from the server\n", __FUNCTION__);
    Disconnect();
  }
  DBG(DBG_PROTO, "%s: %s\n", __FUNCTION__, str.c_str());
  return true;
}

size_t SecureSocket::ReceiveData(void* buf, size_t n)
{
  if (m_connected && n > 0)
  {
    m_ssl_error = SSL_ERROR_NONE;
    for (;;)
    {
      if (SSL_pending(static_cast<SSL*>(m_ssl)) == 0)
      {
        int hangcount = 0;
        for (;;)
        {
          int s = TcpSocket::Listen(&m_timeout);
          if (s > 0)
            break;
          else if (s == 0)
          {
            DBG(DBG_WARN, "%s: socket(%p) timed out (%d)\n", __FUNCTION__, &m_socket, hangcount);
            m_errno = ETIMEDOUT;
            if (++hangcount >= m_attempt)
              return 0;
          }
          else if (m_errno != ERRNO_INTR)
            return 0;
        }
      }

      int r = SSL_read(static_cast<SSL*>(m_ssl), buf, (int) n);
      if (r >= 0)
        return (size_t) r;
      int err = SSL_get_error(static_cast<SSL*>(m_ssl), r);
      if (err == SSL_ERROR_WANT_READ)
      {
        DBG(DBG_DEBUG, "%s: SSL retry\n", __FUNCTION__);
        continue;
      }
      if (err == SSL_ERROR_WANT_WRITE)
      {
        DBG(DBG_DEBUG, "%s: SSL wants write\n", __FUNCTION__);
        m_ssl_error = err;
        break;
      }
      const char* errmsg = ERR_error_string(ERR_get_error(), NULL);
      DBG(DBG_ERROR, "%s: SSL read failed: %s\n", __FUNCTION__, errmsg);
      m_ssl_error = err;
      break;
    }
  }
  return 0;
}

bool SecureSocket::SendData(const char* buf, size_t size)
{
  if (m_connected && size > 0)
  {
    m_ssl_error = SSL_ERROR_NONE;
    for (;;)
    {
      int r = SSL_write(static_cast<SSL*>(m_ssl), buf, (int) size);
      if (r > 0 && size == (size_t) r)
        return true;
      int err = SSL_get_error(static_cast<SSL*>(m_ssl), r);
      if (err == SSL_ERROR_WANT_WRITE)
      {
        DBG(DBG_DEBUG, "%s: SSL retry\n", __FUNCTION__);
        continue;
      }
      if (err == SSL_ERROR_WANT_READ)
      {
        DBG(DBG_DEBUG, "%s: SSL wants read\n", __FUNCTION__);
        m_ssl_error = err;
        break;
      }
      const char* errmsg = ERR_error_string(ERR_get_error(), NULL);
      DBG(DBG_ERROR, "%s: SSL write failed: %s\n", __FUNCTION__, errmsg);
      m_ssl_error = err;
      break;
    }
  }
  return false;
}

void SecureSocket::Disconnect()
{
  if (m_connected)
  {
    SSL_shutdown(static_cast<SSL*>(m_ssl));
    m_connected = false;
  }
  TcpSocket::Disconnect();
  if (m_cert)
  {
    X509_free(static_cast<X509*>(m_cert));
    m_cert = NULL;
  }
}

bool SecureSocket::IsValid() const
{
  return m_connected;
}

bool SecureSocket::IsCertificateValid(std::string& str)
{
  if (m_cert)
    X509_free(static_cast<X509*>(m_cert));
  m_cert = SSL_get_peer_certificate(static_cast<SSL*>(m_ssl));
  if (m_cert)
  {
    char buf[80];
    // X509_get_subject_name() returns the subject name of certificate x.
    // The returned value is an internal pointer which MUST NOT be freed.
    X509_NAME* name = X509_get_subject_name(static_cast<X509*>(m_cert));
    str.assign(X509_NAME_oneline(name, buf, sizeof(buf) - 1));
    return true;
  }
  return false;
}

#else

SSLSessionFactory::SSLSessionFactory()
: m_enabled(false)
, m_ctx(NULL)
{
  DBG(DBG_INFO, "%s: SSL feature is disabled\n", __FUNCTION__);
}

SSLSessionFactory::~SSLSessionFactory()
{
}

SecureSocket* SSLSessionFactory::NewSocket()
{
  return new SecureSocket(NULL);
}

SecureSocket::SecureSocket(void* ssl)
: TcpSocket()
, m_ssl(ssl)
, m_cert(NULL)
, m_connected(false)
, m_ssl_error(0)
{
}

SecureSocket::~SecureSocket()
{
}

bool SecureSocket::Connect(const char* server, unsigned port, int rcvbuf)
{
  (void)server;
  (void)port;
  (void)rcvbuf;
  return false;
}

size_t SecureSocket::ReceiveData(void* buf, size_t n)
{
  (void)buf;
  (void)n;
  return 0;
}

bool SecureSocket::SendData(const char* buf, size_t size)
{
  (void)buf;
  (void)size;
  return false;
}

void SecureSocket::Disconnect()
{
}

bool SecureSocket::IsValid() const
{
  return m_connected;
}

bool SecureSocket::IsCertificateValid(std::string& str)
{
  (void)str;
  return false;
}

#endif

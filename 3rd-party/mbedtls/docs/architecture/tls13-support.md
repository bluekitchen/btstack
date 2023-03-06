TLS 1.3 support
===============

Overview
--------

Mbed TLS provides a partial implementation of the TLS 1.3 protocol defined in
the "Support description" section below. The TLS 1.3 support enablement
is controlled by the MBEDTLS_SSL_PROTO_TLS1_3 configuration option.

The development of the TLS 1.3 protocol is based on the TLS 1.3 prototype
located at https://github.com/hannestschofenig/mbedtls. The prototype is
itself based on a version of the development branch that we aim to keep as
recent as possible (ideally the head) by merging regularly commits of the
development branch into the prototype. The section "Prototype upstreaming
status" below describes what remains to be upstreamed.


Support description
-------------------

- Overview

  - Mbed TLS implements both the client and the server side of the TLS 1.3
    protocol.

  - Mbed TLS supports ECDHE key establishment.

  - Mbed TLS does not support DHE key establishment.

  - Mbed TLS supports pre-shared keys for key establishment, pre-shared keys
    provisioned externally as well as provisioned via the ticket mechanism.

  - Mbed TLS supports session resumption via the ticket mechanism.

  - Mbed TLS does not support sending or receiving early data (0-RTT data).

- Supported cipher suites: depends on the library configuration. Potentially
  all of them:
  TLS_AES_128_GCM_SHA256, TLS_AES_256_GCM_SHA384, TLS_CHACHA20_POLY1305_SHA256,
  TLS_AES_128_CCM_SHA256 and TLS_AES_128_CCM_8_SHA256.

- Supported ClientHello extensions:

  | Extension                    | Support |
  | ---------------------------- | ------- |
  | server_name                  | YES     |
  | max_fragment_length          | no      |
  | status_request               | no      |
  | supported_groups             | YES     |
  | signature_algorithms         | YES     |
  | use_srtp                     | no      |
  | heartbeat                    | no      |
  | apln                         | YES     |
  | signed_certificate_timestamp | no      |
  | client_certificate_type      | no      |
  | server_certificate_type      | no      |
  | padding                      | no      |
  | key_share                    | YES     |
  | pre_shared_key               | YES     |
  | psk_key_exchange_modes       | YES     |
  | early_data                   | no      |
  | cookie                       | no      |
  | supported_versions           | YES     |
  | certificate_authorities      | no      |
  | post_handshake_auth          | no      |
  | signature_algorithms_cert    | no      |


- Supported groups: depends on the library configuration.
  Potentially all ECDHE groups:
  secp256r1, x25519, secp384r1, x448 and secp521r1.

  Finite field groups (DHE) are not supported.

- Supported signature algorithms (both for certificates and CertificateVerify):
  depends on the library configuration.
  Potentially:
  ecdsa_secp256r1_sha256, ecdsa_secp384r1_sha384, ecdsa_secp521r1_sha512,
  rsa_pkcs1_sha256, rsa_pkcs1_sha384, rsa_pkcs1_sha512, rsa_pss_rsae_sha256,
  rsa_pss_rsae_sha384 and rsa_pss_rsae_sha512.

  Note that in absence of an application profile standard specifying otherwise
  rsa_pkcs1_sha256, rsa_pss_rsae_sha256 and ecdsa_secp256r1_sha256 are
  mandatory (see section 9.1 of the specification).

- Supported versions:

  - TLS 1.2 and TLS 1.3 with version negotiation on the client side, not server
    side.

  - TLS 1.2 and TLS 1.3 can be enabled in the build independently of each
    other.

  - If both TLS 1.3 and TLS 1.2 are enabled at build time, only one of them can
    be configured at runtime via `mbedtls_ssl_conf_{min,max}_tls_version` for a
    server endpoint. Otherwise, `mbedtls_ssl_setup` will raise
    `MBEDTLS_ERR_SSL_BAD_CONFIG` error.

- Compatibility with existing SSL/TLS build options:

  The TLS 1.3 implementation is compatible with nearly all TLS 1.2
  configuration options in the sense that when enabling TLS 1.3 in the library
  there is rarely any need to modify the configuration from that used for
  TLS 1.2. There are two exceptions though: the TLS 1.3 implementation requires
  MBEDTLS_PSA_CRYPTO_C and MBEDTLS_SSL_KEEP_PEER_CERTIFICATE, so these options
  must be enabled.

  Most of the Mbed TLS SSL/TLS related options are not supported or not
  applicable to the TLS 1.3 implementation:

  | Mbed TLS configuration option            | Support |
  | ---------------------------------------- | ------- |
  | MBEDTLS_SSL_ALL_ALERT_MESSAGES           | no      |
  | MBEDTLS_SSL_ASYNC_PRIVATE                | no      |
  | MBEDTLS_SSL_CONTEXT_SERIALIZATION        | no      |
  | MBEDTLS_SSL_DEBUG_ALL                    | no      |
  | MBEDTLS_SSL_ENCRYPT_THEN_MAC             | n/a     |
  | MBEDTLS_SSL_EXTENDED_MASTER_SECRET       | n/a     |
  | MBEDTLS_SSL_KEEP_PEER_CERTIFICATE        | no (1)  |
  | MBEDTLS_SSL_RENEGOTIATION                | n/a     |
  | MBEDTLS_SSL_MAX_FRAGMENT_LENGTH          | no      |
  |                                          |         |
  | MBEDTLS_SSL_SESSION_TICKETS              | yes     |
  | MBEDTLS_SSL_SERVER_NAME_INDICATION       | yes     |
  | MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH       | no      |
  |                                          |         |
  | MBEDTLS_ECP_RESTARTABLE                  | no      |
  | MBEDTLS_ECDH_VARIANT_EVEREST_ENABLED     | no      |
  |                                          |         |
  | MBEDTLS_KEY_EXCHANGE_PSK_ENABLED         | n/a (2) |
  | MBEDTLS_KEY_EXCHANGE_DHE_PSK_ENABLED     | n/a     |
  | MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED   | n/a     |
  | MBEDTLS_KEY_EXCHANGE_RSA_PSK_ENABLED     | n/a     |
  | MBEDTLS_KEY_EXCHANGE_RSA_ENABLED         | n/a     |
  | MBEDTLS_KEY_EXCHANGE_DHE_RSA_ENABLED     | n/a     |
  | MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED   | n/a     |
  | MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED | n/a     |
  | MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA_ENABLED  | n/a     |
  | MBEDTLS_KEY_EXCHANGE_ECDH_RSA_ENABLED    | n/a     |
  | MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED     | n/a     |
  |                                          |         |
  | MBEDTLS_PSA_CRYPTO_C                     | no (1)  |
  | MBEDTLS_USE_PSA_CRYPTO                   | yes     |

  (1) These options must remain in their default state of enabled.
  (2) See the TLS 1.3 specific build options section below.

- TLS 1.3 specific build options:

  - MBEDTLS_SSL_TLS1_3_COMPATIBILITY_MODE enables the support for middlebox
    compatibility mode as defined in section D.4 of RFC 8446.

  - MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_ENABLED enables the support for
    the PSK key exchange mode as defined by RFC 8446. If it is the only key
    exchange mode enabled, the TLS 1.3 implementation does not contain any code
    related to key exchange protocols, certificates and signatures.

  - MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED enables the
    support for the ephemeral key exchange mode. If it is the only key exchange
    mode enabled, the TLS 1.3 implementation does not contain any code related
    to PSK based key exchange. The ephemeral key exchange mode requires at least
    one of the key exchange protocol allowed by the TLS 1.3 specification, the
    parsing and validation of x509 certificates and at least one signature
    algorithm allowed by the TLS 1.3 specification for signature computing and
    verification.

  - MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL_ENABLED enables the
    support for the PSK ephemeral key exchange mode. If it is the only key
    exchange mode enabled, the TLS 1.3 implementation does not contain any code
    related to certificates and signatures. The PSK ephemeral key exchange
    mode requires at least one of the key exchange protocol allowed by the
    TLS 1.3 specification.


Prototype upstreaming status
----------------------------

The following parts of the TLS 1.3 prototype remain to be upstreamed:

- Sending (client) and receiving (server) early data (0-RTT data).

- New TLS Message Processing Stack (MPS)

  The TLS 1.3 prototype is developed alongside a rewrite of the TLS messaging layer,
  encompassing low-level details such as record parsing, handshake reassembly, and
  DTLS retransmission state machine.

  MPS has the following components:
  - Layer 1 (Datagram handling)
  - Layer 2 (Record handling)
  - Layer 3 (Message handling)
  - Layer 4 (Retransmission State Machine)
  - Reader  (Abstracted pointer arithmetic and reassembly logic for incoming data)
  - Writer  (Abstracted pointer arithmetic and fragmentation logic for outgoing data)

  Of those components, the following have been upstreamed
  as part of `MBEDTLS_SSL_PROTO_TLS1_3`:

  - Reader ([`library/mps_reader.h`](../../library/mps_reader.h))


Coding rules checklist for TLS 1.3
----------------------------------

The following coding rules are aimed to be a checklist for TLS 1.3 upstreaming
work to reduce review rounds and the number of comments in each round. They
come along (do NOT replace) the project coding rules
(https://mbed-tls.readthedocs.io/en/latest/kb/development/mbedtls-coding-standards). They have been
established and discussed following the review of #4882 that was the
PR upstreaming the first part of TLS 1.3 ClientHello writing code.

TLS 1.3 specific coding rules:

  - TLS 1.3 specific C modules, headers, static functions names are prefixed
    with `ssl_tls13_`. The same applies to structures and types that are
    internal to C modules.

  - TLS 1.3 specific exported functions, structures and types are
    prefixed with `mbedtls_ssl_tls13_`.

  - Use TLS1_3 in TLS 1.3 specific macros.

  - The names of macros and variables related to a field or structure in the
    TLS 1.3 specification should contain as far as possible the field name as
    it is in the specification. If the field name is "too long" and we prefer
    to introduce some kind of abbreviation of it, use the same abbreviation
    everywhere in the code.

    Example 1: #define CLIENT_HELLO_RANDOM_LEN 32, macro for the length of the
        `random` field of the ClientHello message.

    Example 2 (consistent abbreviation): `mbedtls_ssl_tls13_write_sig_alg_ext()`
        and `MBEDTLS_TLS_EXT_SIG_ALG`, `sig_alg` standing for
        `signature_algorithms`.

  - Regarding vectors that are represented by a length followed by their value
    in the data exchanged between servers and clients:

    - Use `<vector name>_len` for the name of a variable used to compute the
      length in bytes of the vector, where <vector name> is the name of the
      vector as defined in the TLS 1.3 specification.

    - Use `p_<vector_name>_len` for the name of a variable intended to hold
      the address of the first byte of the vector length.

    - Use `<vector_name>` for the name of a variable intended to hold the
      address of the first byte of the vector value.

    - Use `<vector_name>_end` for the name of a variable intended to hold
      the address of the first byte past the vector value.

    Those idioms should lower the risk of mis-using one of the address in place
    of another one which could potentially lead to some nasty issues.

    Example: `cipher_suites` vector of ClientHello in
             `ssl_tls13_write_client_hello_cipher_suites()`
    ```
    size_t cipher_suites_len;
    unsigned char *p_cipher_suites_len;
    unsigned char *cipher_suites;
    ```

  - Where applicable, use:
    - the macros to extract a byte from a multi-byte integer MBEDTLS_BYTE_{0-8}.
    - the macros to write in memory in big-endian order a multi-byte integer
      MBEDTLS_PUT_UINT{8|16|32|64}_BE.
    - the macros to read from memory a multi-byte integer in big-endian order
      MBEDTLS_GET_UINT{8|16|32|64}_BE.
    - the macro to check for space when writing into an output buffer
      `MBEDTLS_SSL_CHK_BUF_PTR`.
    - the macro to check for data when reading from an input buffer
      `MBEDTLS_SSL_CHK_BUF_READ_PTR`.

    These macros were introduced after the prototype was written thus are
    likely not to be used in prototype where we now would use them in
    development.

    The three first types, MBEDTLS_BYTE_{0-8}, MBEDTLS_PUT_UINT{8|16|32|64}_BE
    and MBEDTLS_GET_UINT{8|16|32|64}_BE improve the readability of the code and
    reduce the risk of writing or reading bytes in the wrong order.

    The two last types, `MBEDTLS_SSL_CHK_BUF_PTR` and
    `MBEDTLS_SSL_CHK_BUF_READ_PTR`, improve the readability of the code and
    reduce the risk of error in the non-completely-trivial arithmetic to
    check that we do not write or read past the end of a data buffer. The
    usage of those macros combined with the following rule mitigate the risk
    to read/write past the end of a data buffer.

    Examples:
    ```
    hs_hdr[1] = MBEDTLS_BYTE_2( total_hs_len );
    MBEDTLS_PUT_UINT16_BE( MBEDTLS_TLS_EXT_SUPPORTED_VERSIONS, p, 0 );
    MBEDTLS_SSL_CHK_BUF_PTR( p, end, 7 );
    ```

  - To mitigate what happened here
    (https://github.com/Mbed-TLS/mbedtls/pull/4882#discussion_r701704527) from
    happening again, use always a local variable named `p` for the reading
    pointer in functions parsing TLS 1.3 data, and for the writing pointer in
    functions writing data into an output buffer and only that variable. The
    name `p` has been chosen as it was already widely used in TLS code.

  - When an TLS 1.3 structure is written or read by a function or as part of
    a function, provide as documentation the definition of the structure as
    it is in the TLS 1.3 specification.

General coding rules:

  - We prefer grouping "related statement lines" by not adding blank lines
    between them.

    Example 1:
    ```
    ret = ssl_tls13_write_client_hello_cipher_suites( ssl, buf, end, &output_len );
    if( ret != 0 )
        return( ret );
    buf += output_len;
    ```

    Example 2:
    ```
    MBEDTLS_SSL_CHK_BUF_PTR( cipher_suites_iter, end, 2 );
    MBEDTLS_PUT_UINT16_BE( cipher_suite, cipher_suites_iter, 0 );
    cipher_suites_iter += 2;
    ```

  - Use macros for constants that are used in different functions, different
    places in the code. When a constant is used only locally in a function
    (like the length in bytes of the vector lengths in functions reading and
    writing TLS handshake message) there is no need to define a macro for it.

    Example: `#define CLIENT_HELLO_RANDOM_LEN 32`

  - When declaring a pointer the dereferencing operator should be prepended to
    the pointer name not appended to the pointer type:

    Example: `mbedtls_ssl_context *ssl;`

  - Maximum line length is 80 characters.

    Exceptions:

    - string literals can extend beyond 80 characters as we do not want to
      split them to ease their search in the code base.

    - A line can be more than 80 characters by a few characters if just looking
      at the 80 first characters is enough to fully understand the line. For
      example it is generally fine if some closure characters like ";" or ")"
      are beyond the 80 characters limit.

    If a line becomes too long due to a refactoring (for example renaming a
    function to a longer name, or indenting a block more), avoid rewrapping
    lines in the same commit: it makes the review harder. Make one commit with
    the longer lines and another commit with just the rewrapping.

  - When in successive lines, functions and macros parameters should be aligned
    vertically.

    Example:
    ```
    int mbedtls_ssl_start_handshake_msg( mbedtls_ssl_context *ssl,
                                         unsigned hs_type,
                                         unsigned char **buf,
                                         size_t *buf_len );
    ```

  - When a function's parameters span several lines, group related parameters
    together if possible.

    For example, prefer:

    ```
    mbedtls_ssl_start_handshake_msg( ssl, hs_type,
                                     buf, buf_len );
    ```
    over
    ```
    mbedtls_ssl_start_handshake_msg( ssl, hs_type, buf,
                                     buf_len );
    ```
    even if it fits.


Overview of handshake code organization
---------------------------------------

The TLS 1.3 handshake protocol is implemented as a state machine. The
functions `mbedtls_ssl_tls13_handshake_{client,server}_step` are the top level
functions of that implementation. They are implemented as a switch over all the
possible states of the state machine.

Most of the states are either dedicated to the processing or writing of an
handshake message.

The implementation does not go systematically through all states as this would
result in too many checks of whether something needs to be done or not in a
given state to be duplicated across several state handlers. For example, on
client side, the states related to certificate parsing and validation are
bypassed if the handshake is based on a pre-shared key and thus does not
involve certificates.

On the contrary, the implementation goes systematically though some states
even if they could be bypassed if it helps in minimizing when and where inbound
and outbound keys are updated. The `MBEDTLS_SSL_CLIENT_CERTIFICATE` state on
client side is a example of that.

The names of the handlers processing/writing an handshake message are
prefixed with `(mbedtls_)ssl_tls13_{process,write}`. To ease the maintenance and
reduce the risk of bugs, the code of the message processing and writing
handlers is split into a sequence of stages.

The sending of data to the peer only occurs in `mbedtls_ssl_handshake_step`
between the calls to the handlers and as a consequence handlers do not have to
care about the MBEDTLS_ERR_SSL_WANT_WRITE error code. Furthermore, all pending
data are flushed before to call the next handler. That way, handlers do not
have to worry about pending data when changing outbound keys.

### Message processing handlers
For message processing handlers, the stages are:

* coordination stage: check if the state should be bypassed. This stage is
optional. The check is either purely based on the reading of the value of some
fields of the SSL context or based on the reading of the type of the next
message. The latter occurs when it is not known what the next handshake message
will be, an example of that on client side being if we are going to receive a
CertificateRequest message or not. The intent is, apart from the next record
reading to not modify the SSL context as this stage may be repeated if the
next handshake message has not been received yet.

* fetching stage: at this stage we are sure of the type of the handshake
message we must receive next and we try to fetch it. If we did not go through
a coordination stage involving the next record type reading, the next
handshake message may not have been received yet, the handler returns with
`MBEDTLS_ERR_SSL_WANT_READ` without changing the current state and it will be
called again later.

* pre-processing stage: prepare the SSL context for the message parsing. This
stage is optional. Any processing that must be done before the parsing of the
message or that can be done to simplify the parsing code. Some simple and
partial parsing of the handshake message may append at that stage like in the
ServerHello message pre-processing.

* parsing stage: parse the message and restrict as much as possible any
update of the SSL context. The idea of the pre-processing/parsing/post-processing
organization is to concentrate solely on the parsing in the parsing function to
reduce the size of its code and to simplify it.

* post-processing stage: following the parsing, further update of the SSL
context to prepare for the next incoming and outgoing messages. This stage is
optional. For example, secret and key computations occur at this stage, as well
as handshake messages checksum update.

* state change: the state change is done in the main state handler to ease the
navigation of the state machine transitions.


### Message writing handlers
For message writing handlers, the stages are:

* coordination stage: check if the state should be bypassed. This stage is
optional. The check is based on the value of some fields of the SSL context.

* preparation stage: prepare for the message writing. This stage is optional.
Any processing that must be done before the writing of the message or that can
be done to simplify the writing code.

* writing stage: write the message and restrict as much as possible any update
of the SSL context. The idea of the preparation/writing/finalization
organization is to concentrate solely on the writing in the writing function to
reduce the size of its code and simplify it.

* finalization stage: following the writing, further update of the SSL
context to prepare for the next incoming and outgoing messages. This stage is
optional. For example, handshake secret and key computation occur at that
stage (ServerHello writing finalization), switching to handshake keys for
outbound message on server side as well.

* state change: the state change is done in the main state handler to ease
the navigation of the state machine transitions.


Writing and reading early or 0-RTT data
---------------------------------------

An application function to write and send a buffer of data to a server through
TLS may plausibly look like:

```
int write_data( mbedtls_ssl_context *ssl,
                const unsigned char *data_to_write,
                size_t data_to_write_len,
                size_t *data_written )
{
    *data_written = 0;

    while( *data_written < data_to_write_len )
    {
        ret = mbedtls_ssl_write( ssl, data_to_write + *data_written,
                                 data_to_write_len - *data_written );

        if( ret < 0 &&
            ret != MBEDTLS_ERR_SSL_WANT_READ &&
            ret != MBEDTLS_ERR_SSL_WANT_WRITE )
        {
            return( ret );
        }

        *data_written += ret;
    }

    return( 0 );
}
```
where ssl is the SSL context to use, data_to_write the address of the data
buffer and data_to_write_len the number of data bytes. The handshake may
not be completed, not even started for the SSL context ssl when the function is
called and in that case the mbedtls_ssl_write() API takes care transparently of
completing the handshake before to write and send data to the server. The
mbedtls_ssl_write() may not been able to write and send all data in one go thus
the need for a loop calling it as long as there are still data to write and
send.

An application function to write and send early data and only early data,
data sent during the first flight of client messages while the handshake is in
its initial phase, would look completely similar but the call to
mbedtls_ssl_write_early_data() instead of mbedtls_ssl_write().
```
int write_early_data( mbedtls_ssl_context *ssl,
                      const unsigned char *data_to_write,
                      size_t data_to_write_len,
                      size_t *data_written )
{
    *data_written = 0;

    while( *data_written < data_to_write_len )
    {
        ret = mbedtls_ssl_write_early_data( ssl, data_to_write + *data_written,
                                            data_to_write_len - *data_written );

        if( ret < 0 &&
            ret != MBEDTLS_ERR_SSL_WANT_READ &&
            ret != MBEDTLS_ERR_SSL_WANT_WRITE )
        {
            return( ret );
        }

        *data_written += ret;
    }

    return( 0 );
}
```
Note that compared to write_data(), write_early_data() can also return
MBEDTLS_ERR_SSL_CANNOT_WRITE_EARLY_DATA and that should be handled
specifically by the user of write_early_data(). A fresh SSL context (typically
just after a call to mbedtls_ssl_setup() or mbedtls_ssl_session_reset()) would
be expected when calling `write_early_data`.

All together, code to write and send a buffer of data as long as possible as
early data and then as standard post-handshake application data could
plausibly look like:

```
ret = write_early_data( ssl, data_to_write, data_to_write_len,
                        &early_data_written );
if( ret < 0 &&
    ret != MBEDTLS_ERR_SSL_CANNOT_WRITE_EARLY_DATA )
{
    goto error;
}

ret = write_data( ssl, data_to_write + early_data_written,
                  data_to_write_len - early_data_written, &data_written );
if( ret < 0 )
    goto error;

data_written += early_data_written;
```

Finally, taking into account that the server may reject early data, application
code to write and send a buffer of data could plausibly look like:
```
ret = write_early_data( ssl, data_to_write, data_to_write_len,
                        &early_data_written );
if( ret < 0 &&
    ret != MBEDTLS_ERR_SSL_CANNOT_WRITE_EARLY_DATA )
{
    goto error;
}

/*
 * Make sure the handshake is completed as it is a requisite to
 * mbedtls_ssl_get_early_data_status().
 */
while( !mbedtls_ssl_is_handshake_over( ssl ) )
{
    ret = mbedtls_ssl_handshake( ssl );
    if( ret < 0 &&
        ret != MBEDTLS_ERR_SSL_WANT_READ &&
        ret != MBEDTLS_ERR_SSL_WANT_WRITE )
    {
        goto error;
    }
}

ret = mbedtls_ssl_get_early_data_status( ssl );
if( ret < 0 )
    goto error;

if( ret == MBEDTLS_SSL_EARLY_DATA_STATUS_REJECTED )
   early_data_written = 0;

ret = write_data( ssl, data_to_write + early_data_written,
                  data_to_write_len - early_data_written, &data_written );
if( ret < 0 )
    goto error;

data_written += early_data_written;
```

Basically, the same holds for reading early data on the server side without the
complication of possible rejection. An application function to read early data
into a given buffer could plausibly look like:
```
int read_early_data( mbedtls_ssl_context *ssl,
                     unsigned char *buffer,
                     size_t buffer_size,
                     size_t *data_len )
{
    *data_len = 0;

    while( *data_len < buffer_size )
    {
        ret = mbedtls_ssl_read_early_data( ssl, buffer + *data_len,
                                           buffer_size - *data_len );

        if( ret < 0 &&
            ret != MBEDTLS_ERR_SSL_WANT_READ &&
            ret != MBEDTLS_ERR_SSL_WANT_WRITE )
        {
            return( ret );
        }

        *data_len += ret;
    }

    return( 0 );
}
```
with again calls to read_early_data() expected to be done with a fresh SSL
context.

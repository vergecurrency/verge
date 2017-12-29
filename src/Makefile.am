AM_CPPFLAGS =  $(INCLUDES) 
AM_LDFLAGS = $(PTHREAD_CFLAGS)


if EMBEDDED_LEVELDB
LEVELDB_CPPFLAGS += -I$(srcdir)/leveldb/include
LEVELDB_CPPFLAGS += -I$(srcdir)/leveldb/helpers/memenv
LIBLEVELDB += $(builddir)/leveldb/libleveldb.a
LIBMEMENV  += $(builddir)/leveldb/libmemenv.a

# NOTE: This dependency is not strictly necessary, but without it make may try to build both in parallel, which breaks the LevelDB build system in a race
$(LIBLEVELDB): $(LIBMEMENV)

$(LIBLEVELDB) $(LIBMEMENV):
	@echo "Building LevelDB ..." && $(MAKE) -C $(@D) $(@F) CXX="$(CXX)" \
	  CC="$(CC)" PLATFORM=$(TARGET_OS) AR="$(AR)" $(LEVELDB_TARGET_FLAGS) \
          OPT="$(CXXFLAGS) $(CPPFLAGS)"
endif

BITCOIN_CONFIG_INCLUDES=-I$(builddir)/config
BITCOIN_INCLUDES=-I$(builddir) -I$(builddir)/obj $(BOOST_CPPFLAGS) $(LEVELDB_CPPFLAGS)

LIBBITCOIN_SERVER=libbitcoin_server.a
LIBBITCOIN_WALLET=libbitcoin_wallet.a
LIBBITCOIN_COMMON=libbitcoin_common.a
LIBBITCOIN_UTIL=libbitcoin_util.a
if HAVE_SSE2
LIBBITCOIN_SSE2=libbitcoin_sse2.a
endif

LIBBITCOINQT=qt/libbitcoinqt.a

noinst_LIBRARIES = \
  libbitcoin_server.a \
  libbitcoin_common.a \
  libbitcoin_util.a
if ENABLE_WALLET
BITCOIN_INCLUDES += $(BDB_CPPFLAGS)
noinst_LIBRARIES += libbitcoin_wallet.a
endif

if HAVE_SSE2
noinst_LIBRARIES += libbitcoin_sse2.a
endif

bin_PROGRAMS =
TESTS =

if BUILD_BITCOIND
  bin_PROGRAMS += VERGEd
endif

.PHONY: FORCE
# bitcoin core #
BITCOIN_CORE_H = \
  addrman.h \
  alert.h \
  allocators.h \
  base58.h \
  bignum.h \
  bitcoinrpc.h \
  checkpoints.h \
  clientversion.h \
  compat.h \
  crypter.h \
  smessage.h \
  xxhash/xxhash.h \
  sph_groestl.h \
  sph_types.h \
  sph_blake.h \
  sph_bmw.h \
  sph_jh.h \
  sph_keccak.h \
  sph_skein.h \
  sph_luffa.h \
  sph_cubehash.h \
  sph_simd.h \
  sph_echo.h \
  sph_shavite.h \
  sph_sha2.h \
  sph_haval.h \
  hashgroestl.h \
  hashskein.h \
  hashqubit.h \
  hashquark.h \
  hashblake.h \
  hashx17.h \
  hashx13.h \
  hashx15.h \
  Lyra2RE.h \
  Lyra2.h \
  Sponge.h \
  blake2.h \
  blake2-impl.h \
  db.h \
  txdb-leveldb.h \
  init.h \
  kernel.h \
  key.h \
  keystore.h \
  lz4/lz4.h \
  main.h \
  mruset.h \
  netbase.h \
  net.h \
  protocol.h \
  script.h \
  scrypt.h \
  serialize.h \
  strlcpy.h \
  sync.h \
  ui_interface.h \
  uint256.h \
  util.h \
  hash.h \
  version.h \
  walletdb.h \
  wallet.h \
  src/qt/httpsocket.h \
  src/qt/stealthsend.h 

JSON_H = \
  json/json_spirit.h \
  json/json_spirit_error_position.h \
  json/json_spirit_reader.h \
  json/json_spirit_reader_template.h \
  json/json_spirit_stream_reader.h \
  json/json_spirit_utils.h \
  json/json_spirit_value.h \
  json/json_spirit_writer.h \
  json/json_spirit_writer_template.h

obj/build.h: FORCE
	@$(MKDIR_P) $(builddir)/obj
	@$(top_srcdir)/share/genbuild.sh $(abs_top_builddir)/src/obj/build.h \
	  $(abs_top_srcdir)
libbitcoin_util_a-version.$(OBJEXT): obj/build.h

# server: shared between bitcoind and bitcoin-qt
libbitcoin_server_a_CPPFLAGS = $(BITCOIN_INCLUDES)
libbitcoin_server_a_SOURCES = \
  addrman.cpp \
  alert.cpp \
  checkpoints.cpp \
  init.cpp \
  db.cpp \
  smessage.cpp \
  txdb-leveldb.cpp \
  bitcoinrpc.cpp \
  keystore.cpp \
  main.cpp \
  noui.cpp \
  net.cpp \
  rpcblockchain.cpp \
  rpcmining.cpp \
  rpcnet.cpp \
  rpcsmessage.cpp \
  rpcrawtransaction.cpp \
  src/tor/address.c \
  src/tor/addressmap.c \
  src/tor/aes.c \
  src/tor/anonymize.cpp \
  src/tor/backtrace.c \
  src/tor/buffers.c \
  src/tor/channel.c \
  src/tor/channeltls.c \
  src/tor/circpathbias.c \
  src/tor/circuitbuild.c \
  src/tor/circuitlist.c \
  src/tor/circuitmux.c \
  src/tor/circuitmux_ewma.c \
  src/tor/circuitstats.c \
  src/tor/circuituse.c \
  src/tor/command.c \
  src/tor/compat.c \
  src/tor/compat_libevent.c \
  src/tor/config.c \
  src/tor/config_codedigest.c \
  src/tor/confparse.c \
  src/tor/connection.c \
  src/tor/connection_edge.c \
  src/tor/connection_or.c \
  src/tor/container.c \
  src/tor/control.c \
  src/tor/cpuworker.c \
  src/tor/crypto.c \    
  src/tor/crypto_curve25519.c \
  src/tor/crypto_format.c \
  src/tor/curve25519-donna.c \
  src/tor/di_ops.c \
  src/tor/directory.c \
  src/tor/dirserv.c \
  src/tor/dirvote.c \
  src/tor/dns.c \
  src/tor/dnsserv.c \
  src/tor/entrynodes.c \
  src/tor/ext_orport.c \
  src/tor/fp_pair.c \
  src/tor/geoip.c \
  src/tor/hibernate.c \
  src/tor/log.c \
  src/tor/memarea.c \
  src/tor/mempool.c \
  src/tor/microdesc.c \
  src/tor/networkstatus.c \
  src/tor/nodelist.c \
  src/tor/onion.c \
  src/tor/onion_fast.c \
  src/tor/onion_main.c \
  src/tor/onion_ntor.c \
  src/tor/onion_tap.c \
  src/tor/policies.c \
  src/tor/stealth.cpp \
  src/tor/procmon.c \
  src/tor/reasons.c \
  src/tor/relay.c \
  src/tor/rendclient.c \
  src/tor/rendcommon.c \
  src/tor/rendmid.c \
  src/tor/rendservice.c \
  src/tor/rephist.c \
  src/tor/replaycache.c \
  src/tor/router.c \
  src/tor/routerlist.c \
  src/tor/routerparse.c \
  src/tor/routerset.c \
  src/tor/sandbox.c \
  src/tor/statefile.c \
  src/tor/status.c \
  src/tor/strlcat.c \
  src/tor/strlcpy.c \
  src/tor/tor_util.c \
  src/tor/torgzip.c \
  src/tor/tortls.c \
  src/tor/transports.c \
  src/tor/util_codedigest.c \
  $(JSON_H) \
  $(BITCOIN_CORE_H)

# wallet: shared between bitcoind and bitcoin-qt, but only linked
# when wallet enabled
libbitcoin_wallet_a_CPPFLAGS = $(BITCOIN_INCLUDES)
libbitcoin_wallet_a_SOURCES = \
  crypter.cpp \
  rpcdump.cpp \
  rpcwallet.cpp \
  wallet.cpp \
  walletdb.cpp \
  src/qt/httpsocket.cpp \
  src/qt/stealthsend.cpp \
  $(BITCOIN_CORE_H)

# common: shared between bitcoind, and bitcoin-qt and non-server tools
libbitcoin_common_a_CPPFLAGS = $(BITCOIN_INCLUDES)
libbitcoin_common_a_SOURCES = \
  key.cpp \
  netbase.cpp \
  protocol.cpp \
  script.cpp \
  kernel.cpp \
  scrypt.cpp \
  groestl.c \
  blake.c \
  blake2s-ref.c \
  bmw.c \
  cubehash.c \
  echo.c \
  jh.c \
  keccak.c \
  luffa.c \
  shavite.c \
  simd.c \
  skein.c \
  hamsi.c \
  whirlpool.c \
  shabal.c \
  fugue.c \
  sync.cpp \
  sha2.c \
  haval.c \
  Lyra2.c \
  Lyra2RE.c \
  Sponge.c \
  sph_sha2big.c \
  $(BITCOIN_CORE_H)

# util: shared between all executables.
# This library *must* be included to make sure that the glibc
# backward-compatibility objects and their sanity checks are linked.
libbitcoin_util_a_CPPFLAGS = $(BITCOIN_INCLUDES)
libbitcoin_util_a_SOURCES = \
  util.cpp \
  version.cpp \
  compat/glibc_sanity.cpp \
  compat/glibcxx_sanity.cpp \
  lz4/lz4.c \
  xxhash/xxhash.c \
  $(BITCOIN_CORE_H)

if GLIBC_BACK_COMPAT
libbitcoin_util_a_SOURCES += compat/glibc_compat.cpp
libbitcoin_util_a_SOURCES += compat/glibcxx_compat.cpp
endif

nodist_libbitcoin_util_a_SOURCES = $(srcdir)/obj/build.h

# bitcoind binary #
VERGEd_LDADD = \
  $(LIBBITCOIN_SERVER) \
  $(LIBBITCOIN_COMMON) \
  $(LIBBITCOIN_UTIL) \
  $(LIBLEVELDB) \
  $(LIBMEMENV)

if HAVE_SSE2
libbitcoin_sse2_a_SOURCES = scrypt-sse2.cpp
libbitcoin_sse2_a_CPPFLAGS = $(BITCOIN_INCLUDES) -msse2 -mstackrealign
VERGEd_LDADD += libbitcoin_sse2.a
endif

if ENABLE_WALLET
VERGEd_LDADD += libbitcoin_wallet.a
endif
VERGEd_SOURCES = daemon.cpp

VERGEd_LDADD += $(BOOST_LIBS) $(BDB_LIBS)
VERGEd_CPPFLAGS = $(BITCOIN_INCLUDES)

CLEANFILES = leveldb/libleveldb.a leveldb/libmemenv.a *.gcda *.gcno

DISTCLEANFILES = obj/build.h

EXTRA_DIST = leveldb

clean-local:
	-$(MAKE) -C leveldb clean
	rm -f leveldb/*/*.gcno leveldb/helpers/memenv/*.gcno
	-rm -f config.h

.rc.o:
	@test -f $(WINDRES)
	$(AM_V_GEN) $(WINDRES) -i $< -o $@

.mm.o:
	$(AM_V_CXX) $(OBJCXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) \
	  $(CPPFLAGS) $(AM_CXXFLAGS) $(QT_INCLUDES) $(CXXFLAGS)  -c -o $@ $<

%.pb.cc %.pb.h: %.proto
	@test -f $(PROTOC)
	$(AM_V_GEN) $(PROTOC) --cpp_out=$(@D) --proto_path=$(abspath $(<D) $<)

if ENABLE_QT
include Makefile.qt.include
endif

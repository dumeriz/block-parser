project('block-parser', ['cpp', 'c'], default_options : ['cpp_std=c++17'])

znn_src = files('zenon/crypto/hmac_sha256.cpp', 'zenon/crypto/sha1.cpp', 'zenon/crypto/sha512.cpp', 'zenon/crypto/hmac_sha512.cpp', 'zenon/crypto/sha256.cpp', 'zenon/crypto/rfc6979_hmac_sha256.cpp', 'zenon/crypto/scrypt.cpp', 'zenon/crypto/ripemd160.cpp', 'zenon/utilstrencodings.cpp', 'zenon/allocators.cpp', 'zenon/uint256.cpp', 'zenon/cleanse.cpp', 'zenon/crypto/keccak.c', 'zenon/crypto/aes_helper.c', 'zenon/crypto/simd.c', 'zenon/crypto/luffa.c', 'zenon/crypto/blake.c', 'zenon/crypto/cubehash.c', 'zenon/crypto/jh.c', 'zenon/crypto/shavite.c', 'zenon/crypto/groestl.c', 'zenon/crypto/bmw.c', 'zenon/crypto/skein.c', 'zenon/crypto/echo.c')
znn_inc = include_directories('.', 'zenon/')

ssl_dep = dependency('openssl')
redis_dep = dependency('cpp_redis')
tacopie_dep = dependency('tacopie')
thread_dep = dependency('threads')

# This part might help to build on macos - it's not working out of the box probably, but the idea should be clear
# compiler = meson.get_compiler('cpp')
# crypto_dep = compiler.find_library('crypto', dirs : '/opt/local/lib')
# ssl_dep = compiler.find_library('ssl', dirs : '/opt/local/lib')
# redis_inc = include_directories('cpp_redis/includes')
# tacopie_inc = include_directories('cpp_redis/tacopie/includes')
# redis_dep = compiler.find_library('cpp_redis', dirs : meson.source_root() + '/cpp_redis/build/lib')
# tacopie_dep = compiler.find_library('tacopie', dirs : meson.source_root() + '/cpp_redis/build/lib')

src = files('src/main.cpp', 'src/header.cpp', 'src/block.cpp', 'src/transaction.cpp', 'src/tx_out.cpp', 'src/tx_in.cpp')
inc = include_directories('include')

executable('block-parser',
    sources : [src, znn_src],
    include_directories : [inc, znn_inc],
    dependencies: [ssl_dep, thread_dep, redis_dep, tacopie_dep]
)
 

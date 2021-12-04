#
# The basics
#
CC=clang
CXX=clang++
STRIP=strip
CFLAGS=
CXXFLAGS=-Wall -Wno-unknown-pragmas -std=c++17 -I./src -I./vendor
LDFLAGS=-lpthread -lsqlite3
SERVER=bin/fusion

# protocol version
# this can be overriden by ex. make PROTOCOL_VERSION=728
PROTOCOL_VERSION=104

#
# Configuration options
#
CONFIG_DEBUG=no
CONFIG_STRIP=no
CONFIG_HARDENING=yes
# If compiling with ASAN, invoke like this: $ LSAN_OPTIONS=suppressions=suppr.txt bin/fusion
CONFIG_ASAN=no
CONFIG_GPROF=no
CONFIG_WINDOWS=no
CONFIG_32BIT=no

#
# Advanced configuration
#
DEBUG_CFLAGS=-O0 -g3
DEBUG_LDFLAGS=-O0 -g3
# if not debug
RELEASE_CFLAGS=-O2
RELEASE_LDFLAGS=-O2

HARDENING_CFLAGS=-fPIE -D_FORTIFY_SOURCE=1 -fstack-protector
HARDENING_LDFLAGS=-pie -Wl,-z,relro -Wl,-z,now

ASAN_CFLAGS=-fsanitize=address
ASAN_LDFLAGS=-fsanitize=address

GPROF_CFLAGS=-pg
GPROF_LDFLAGS=-pg

WIN_CC=x86_64-w64-mingw32-gcc
WIN_CXX=x86_64-w64-mingw32-g++
WIN_STRIP=x86_64-w64-mingw32-strip
# these CFLAGS are added onto the base ones
WIN_CFLAGS=-D_WIN32_WINNT=0x0601
# these LDFLAGS replace the base ones
WIN_LDFLAGS=-static -lws2_32 -lwsock32 -lsqlite3
WIN_SERVER=bin/winfusion.exe

32BIT_CFLAGS=-m32
32BIT_LDFLAGS=-m32

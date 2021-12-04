include config.mk

GIT_VERSION!=git describe --tags

CXXFLAGS+=-DPROTOCOL_VERSION=$(PROTOCOL_VERSION) -DGIT_VERSION=\"$(GIT_VERSION)\"

# apply Windows overrides
WIN_OVERRIDE-yes=no
WIN_OVERRIDE-$(CONFIG_WINDOWS)=

CC$(WIN_OVERRIDE-yes)=$(WIN_CC)
CXX$(WIN_OVERRIDE-yes)=$(WIN_CXX)
STRIP$(WIN_OVERRIDE-yes)=$(WIN_STRIP)
LDFLAGS$(WIN_OVERRIDE-yes)=$(WIN_LDFLAGS)
SERVER$(WIN_OVERRIDE-yes)=$(WIN_SERVER)
# append CFLAGS, instead of overriding
CFLAGS-$(CONFIG_WINDOWS)+=$(WIN_CFLAGS)

# CONFIG_RELEASE is the inverse of CONFIG_DEBUG
RELEASE_OVERRIDE-yes=
RELEASE_OVERRIDE-$(CONFIG_DEBUG)=no

CFLAGS-yes$(RELEASE_OVERRIDE-yes)+=$(RELEASE_CFLAGS)
LDFLAGS-yes$(RELEASE_OVERRIDE-yes)+=$(RELEASE_LDFLAGS)

# strip resulting binary if requested
STRIP_OVERRIDE-yes=
STRIP_OVERRIDE-$(CONFIG_STRIP)=no
STRIP$(STRIP_OVERRIDE-yes)=true

# the remaining flag tweaks
CFLAGS-$(CONFIG_DEBUG)+=$(DEBUG_CFLAGS)
LDFLAGS-$(CONFIG_DEBUG)+=$(DEBUG_LDFLAGS)
CFLAGS-$(CONFIG_HARDENING)+=$(HARDENING_CFLAGS)
LDFLAGS-$(CONFIG_HARDENING)+=$(HARDENING_LDFLAGS)
CFLAGS-$(CONFIG_GPROF)+=$(GPROF_CFLAGS)
LDFLAGS-$(CONFIG_GPROF)+=$(GPROF_LDFLAGS)
CFLAGS-$(CONFIG_ASAN)+=$(ASAN_CFLAGS)
LDFLAGS-$(CONFIG_ASAN)+=$(ASAN_LDFLAGS)
CFLAGS-$(CONFIG_32BIT)+=$(32BIT_CFLAGS)
LDFLAGS-$(CONFIG_32BIT)+=$(32BIT_LDFLAGS)

# apply
CFLAGS+=$(CFLAGS-yes)
LDFLAGS+=$(LDFLAGS-yes)

CXXFLAGS+=$(CFLAGS)

# C code; currently exclusively from vendored libraries
CSRC=\
	vendor/bcrypt/bcrypt.c\
	vendor/bcrypt/crypt_blowfish.c\
	vendor/bcrypt/crypt_gensalt.c\
	vendor/bcrypt/wrapper.c\

CHDR=\
	vendor/bcrypt/bcrypt.h\
	vendor/bcrypt/crypt_blowfish.h\
	vendor/bcrypt/crypt_gensalt.h\
	vendor/bcrypt/ow-crypt.h\
	vendor/bcrypt/winbcrypt.h\

CXXSRC=\
	src/core/CNProtocol.cpp\
	src/core/CNShared.cpp\
	src/core/Packets.cpp\
	src/servers/CNLoginServer.cpp\
	src/servers/CNShardServer.cpp\
	src/servers/Monitor.cpp\
	src/db/init.cpp\
	src/db/login.cpp\
	src/db/shard.cpp\
	src/db/player.cpp\
	src/db/email.cpp\
	src/Chat.cpp\
	src/CustomCommands.cpp\
	src/Entities.cpp\
	src/Email.cpp\
	src/Eggs.cpp\
	src/main.cpp\
	src/Missions.cpp\
	src/MobAI.cpp\
	src/Combat.cpp\
	src/Nanos.cpp\
	src/Abilities.cpp\
	src/Items.cpp\
	src/NPCManager.cpp\
	src/PlayerManager.cpp\
	src/PlayerMovement.cpp\
	src/BuiltinCommands.cpp\
	src/settings.cpp\
	src/Transport.cpp\
	src/TableData.cpp\
	src/Chunking.cpp\
	src/Buddies.cpp\
	src/Groups.cpp\
	src/Racing.cpp\
	src/Vendors.cpp\
	src/Trading.cpp\
	src/Rand.cpp\

# headers (for timestamp purposes)
CXXHDR=\
	src/core/CNProtocol.hpp\
	src/core/CNShared.hpp\
	src/core/CNStructs.hpp\
	src/core/Packets.hpp\
	src/core/Defines.hpp\
	src/core/Core.hpp\
	src/servers/CNLoginServer.hpp\
	src/servers/CNShardServer.hpp\
	src/servers/Monitor.hpp\
	src/db/Database.hpp\
	src/db/internal.hpp\
	vendor/bcrypt/BCrypt.hpp\
	vendor/INIReader.hpp\
	vendor/JSON.hpp\
	vendor/INIReader.hpp\
	vendor/JSON.hpp\
	src/Chat.hpp\
	src/CustomCommands.hpp\
	src/Entities.hpp\
	src/Email.hpp\
	src/Eggs.hpp\
	src/Missions.hpp\
	src/MobAI.hpp\
	src/Combat.hpp\
	src/Nanos.hpp\
	src/Abilities.hpp\
	src/Items.hpp\
	src/NPCManager.hpp\
	src/Player.hpp\
	src/PlayerManager.hpp\
	src/PlayerMovement.hpp\
	src/BuiltinCommands.hpp\
	src/settings.hpp\
	src/Transport.hpp\
	src/TableData.hpp\
	src/Chunking.hpp\
	src/Buddies.hpp\
	src/Groups.hpp\
	src/Racing.hpp\
	src/Vendors.hpp\
	src/Trading.hpp\
	src/Rand.hpp\

COBJ=$(CSRC:.c=.o)
CXXOBJ=$(CXXSRC:.cpp=.o)

OBJ=$(COBJ) $(CXXOBJ)

HDR=$(CHDR) $(CXXHDR)

all: $(SERVER)

.SUFFIXES: .o .c .cpp .h .hpp

.PHONY: all windows clean nuke

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

.cpp.o:
	$(CXX) -c $(CXXFLAGS) -o $@ $<

# header timestamps are a prerequisite for OF object files
$(CXXOBJ): $(HDR)

$(SERVER): $(OBJ) $(CHDR) $(CXXHDR)
	mkdir -p bin
	$(CXX) $(OBJ) $(LDFLAGS) -o $(SERVER)
	$(STRIP) $(SERVER)

# compatibility with how cmake injects GIT_VERSION
version.h:
	touch version.h

src/main.o: version.h

# only gets rid of OpenFusion objects, so we don't need to
# recompile the libs every time
clean:
	rm -f src/*.o src/*/*.o $(SERVER) $(WIN_SERVER) version.h

# gets rid of all compiled objects, including the libraries
nuke:
	rm -f $(OBJ) $(SERVER) $(WIN_SERVER) version.h

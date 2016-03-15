# Copyright 2016 Gauthier Voron <gauthier.voron@lip6.fr>
# This file is part of pin.
#
# Pin is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Pin is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with pin.  If not, see <http://www.gnu.org/licenses/>.

SRC := src/
TST := t/
INC := include/
OBJ := obj/
LIB := lib/
BIN := bin/

VERSION := 1.0.0
AUTHOR  := Gauthier Voron
EMAIL   := gauthier.voron@lip6.fr

CC      := gcc
CCFLAGS := -Wall -Wextra -O2 -g -DVERSION='"$(VERSION)"' \
           -DAUTHOR='"$(AUTHOR)"' -DEMAIL='"$(EMAIL)"'
SOFLAGS := -fPIC -shared
LDFLAGS := -ldl -lpthread

pin-obj     := argument error runtime
pin-lib     := -ldl -lpthread
scanpin-lib := -lrt
pthread-lib := -lpthread -lrt


V ?= 1
ifeq ($(V),1)
  define print
    @echo '$(1)'
  endef
endif
ifneq ($(V),2)
  Q := @
endif


default: all

all: $(LIB)pin.so $(BIN)scanpin
check: $(LIB)pin.so $(BIN)pthread
	$(call print,  CHECK   $(TST)check.sh)
	$(Q)./$(TST)check.sh $(LIB)pin.so $(BIN)


$(LIB)pin.so: $(patsubst %, $(OBJ)%.so, $(pin-obj)) | $(LIB)
	$(call print,  LD      $@)
	$(Q)$(CC) $(SOFLAGS) $^ -o $@ $(pin-lib)

$(BIN)scanpin: $(SRC)scanpin.c | $(BIN)
	$(call print,  CCLD    $@)
	$(Q)$(CC) $(CCFLAGS) -DPROGNAME='"scanpin"' $< -o $@ $(scanpin-lib)

$(BIN)%: $(TST)%.c | $(BIN)
	$(call print,  CCLD    $@)
	$(Q)$(CC) $(CCFLAGS) $< -o $@ $($(patsubst $(BIN)%,%,$@)-lib)


$(OBJ)%.so: $(SRC)%.c | $(OBJ)
	$(call print,  CC      $@)
	$(Q)$(CC) -c $(CCFLAGS) -I$(INC) $(SOFLAGS) $< -o $@


$(OBJ) $(LIB) $(BIN):
	$(call print,  MKDIR   $@)
	$(Q)mkdir $@


clean:
	$(call print,  CLEAN)
	$(Q)-rm -rf $(OBJ) $(LIB) $(BIN)

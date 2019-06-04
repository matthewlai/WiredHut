##
## This file is part of the WiredHut project.
##
## Copyright (C) 2019 Matthew Lai <m@matthewlai.ca>
##
## This library is free software: you can redistribute it and/or modify
## it under the terms of the GNU Lesser General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This library is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU Lesser General Public License for more details.
##
## You should have received a copy of the GNU Lesser General Public License
## along with this library.  If not, see <http://www.gnu.org/licenses/>.
##

###### Project configuration ######
# Name of the main binary
BINARY = garden

# .c, .cpp, and .cxx files
SRCS = $(wildcard src/*.cpp)

# Directories containing header files
INCLUDE = include .

# Chip part number
DEVICE = stm32f767zit6u


###### DO NOT CHANGE BELOW THIS LINE ######
ifeq ($(strip $(OSTRICH_PATH)),)
$(error OSTRICH_PATH undefined!)
endif

include $(OSTRICH_PATH)/Makefiles/rules.mk

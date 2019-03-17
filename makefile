OBJDIR = tmp
LDIR = lib
# OBJ = $(OBJDIR)/main.o $(OBJDIR)/connmgr.o $(OBJDIR)/datamgr.o $(OBJDIR)/sensor_db.o $(OBJDIR)/sbuffer.o
# can also be written as :
OBJ = $(addprefix $(OBJDIR)/,main.o connmgr.o datamgr.o sensor_db.o sbuffer.o)
CC = gcc
CFLAGS = -Wall -std=c11 -Werror -c -fdiagnostics-color=auto -DSET_MIN_TEMP=10 -DSET_MAX_TEMP=20 -DTIMEOUT=5 -DDEBUG
LFLAGS = -Wall -fdiagnostics-color=auto -L./$(LDIR) -Wl,-rpath=./$(LDIR) -lsqlite3
CDFLAGS = -Wall -std=c11 -Werror -c -fdiagnostics-color=auto -fPIC
LDFLAGS = -Wall -fdiagnostics-color=auto -shared -lm

TITLE_COLOR = \033[33m
NO_COLOR = \033[0m
.SUFFIXES:

###
#
# Info on automatic variables :
# $@ : The name of the target of the rule (first part, before the colon)
# $< : The name of the first prerequisite (first dependency, after the colon)
# $^ : The names of all the prerequisites (everything after the colon)
# $* : The matched pattern by % in the target of the rule (aka the stem)
#
###

# when executing make, compile all exe's
all: sensor_gateway sensor_node file_creator

# When trying to compile one of the executables, first look for its .o file (in the tmp folder)
# Then check if the libraries are in the lib folder
# % is a wildcard : e.g. gateway will look for temp/gateway.o
sensor_gateway : $(OBJ) $(LDIR)/libdplist.so $(LDIR)/libtcpsock.so
	@echo "$(TITLE_COLOR)\n***** LINKING $< *****$(NO_COLOR)"
#when all dependencies were ok, link the executable
	$(CC) $(OBJ) -ldplist -ltcpsock -lpthread -o $@ $(LFLAGS)

file_creator : $(OBJDIR)/file_creator.o
	@echo "$(TITLE_COLOR)\n***** LINKING $< *****$(NO_COLOR)"
	$(CC) $< -o $@ $(LFLAGS)
	
sensor_node : $(OBJDIR)/sensor_node.o $(LDIR)/libtcpsock.so
	@echo "$(TITLE_COLOR)\n***** LINKING $< *****$(NO_COLOR)"
	$(CC) $< -ltcpsock -o $@ $(LFLAGS)

# If you only want to compile one of the libs, this target will match (e.g. make liblist)
libdplist libsbuffer libtcpsock : % : $(LDIR)/%.so

# When a lib needs to be compiled, this target will do the job
$(LDIR)/lib%.so : $(LDIR)/%.c
# Extract the lib name (e.g. libdplist.so will look for dplist.c, so we store 'dplist' in the variable 'name')
	@echo "$(TITLE_COLOR)\n***** COMPILING LIB $< *****$(NO_COLOR)"
	mkdir -p $(OBJDIR)
# Compile the lib to the tmp folder
	$(CC) $(CDFLAGS) -o$(OBJDIR)/$*.o $< 
	mkdir -p $(LDIR)
	@echo "$(TITLE_COLOR)\n***** LINKING LIB $< *****$(NO_COLOR)"
# Link the .o file from the tmp folder to an .so file in the lib folder
	$(CC) $(OBJDIR)/$*.o -o $(LDIR)/lib$*.so $(LDFLAGS)

# When a certain .o file is not found in the tmp folder, compile it from the .c file, and do a cppcheck
# % is a wildcard : e.g. when compiling tmp/gateway.o he will look for gateway.c
$(OBJDIR)/%.o : %.c %.check
	@echo "$(TITLE_COLOR)\n***** COMPILING $< *****$(NO_COLOR)"
# make the tmp folder if it is not there
	mkdir -p $(OBJDIR)
# Compile ! (mind that $@ is holding $(OBJDIR)/%.o so it will end up in the tmp folder)
	$(CC) $(CFLAGS) -o$@ $< 

# do not look for files called clean, clean-all or *.check this will be always a target
.PHONY : clean clean-all *.check

%.check : %.c
	@echo "$(TITLE_COLOR)\n***** CPPCHECK $< *****$(NO_COLOR)"
	cppcheck --enable=all --suppress=missingIncludeSystem $^

clean:
	rm -rf *.o $(EXE) *~ $(OBJDIR)

clean-all: clean
	rm -rf $(LDIR)/*.so $(UTILS)
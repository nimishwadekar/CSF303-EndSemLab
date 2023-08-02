ROUTER_BIN := bin/router
APP_BIN := bin/app
CRYPT_BIN := bin/crypt

BACKGROUND_SRC := src/_background
ENCRYPTED_LOG_SRC := $(BACKGROUND_SRC)/encrlog_c
CRYPT_SRC := $(BACKGROUND_SRC)/crypt.c
LOG_SRC := $(BACKGROUND_SRC)/log.c
COMMON_SRC := src/packet.c $(BACKGROUND_SRC)/common.c $(BACKGROUND_SRC)/log.c
ROUTER_SRC := src/router.c $(BACKGROUND_SRC)/router_driver.c $(BACKGROUND_SRC)/packet_test.c $(COMMON_SRC)
APP_SRC := src/application.c $(BACKGROUND_SRC)/application_driver.c $(COMMON_SRC)

FLAGS := -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -pthread

route:
	@echo \*** COMPILING ROUTER \***
	@mkdir -p bin
	@mkdir -p log
	@gcc $(CRYPT_SRC) -o $(CRYPT_BIN)
	@./$(CRYPT_BIN) $(ENCRYPTED_LOG_SRC) $(LOG_SRC)
	gcc $(FLAGS) $(ROUTER_SRC) -o $(ROUTER_BIN)
	@rm $(CRYPT_BIN) $(LOG_SRC)
	@echo
	@echo \*** RUNNING ROUTER \***
	@echo ===============================================
	@echo
	@./$(ROUTER_BIN) $(IP) ; \
	echo
	@echo ===============================================

app:
	@echo \*** COMPILING APPLICATION \***
	@mkdir -p bin
	@mkdir -p log
	@gcc $(CRYPT_SRC) -o $(CRYPT_BIN)
	@./$(CRYPT_BIN) $(ENCRYPTED_LOG_SRC) $(LOG_SRC)
	gcc $(FLAGS) $(APP_SRC) -o $(APP_BIN)
	@rm $(CRYPT_BIN) $(LOG_SRC)
	@echo
	@echo \*** RUNNING APPLICATION \***
	@echo ===============================================
	@echo
	@./$(APP_BIN) ; \
	echo
	@echo ===============================================

clean:
	rm -f bin/*

clean_log:
	rm -f log/*

encrypt:
	@mkdir -p bin
	@gcc $(CRYPT_SRC) -o $(CRYPT_BIN)
	@./$(CRYPT_BIN) $(BACKGROUND_SRC)/_log.c $(ENCRYPTED_LOG_SRC)
	@rm $(CRYPT_BIN)

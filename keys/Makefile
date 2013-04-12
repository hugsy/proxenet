CA_KEY_PREFIX	=	"proxenet"
CA_KEY_SIZE	=	1024
CA_KEY_DURATION	= 	3650

.PHONY :  all keys

keys:
	@echo "[+] Generating new private key"
	@openssl genrsa -des3 -out $(CA_KEY_PREFIX).key $(CA_KEY_SIZE)
	@echo "[+] Creating CSR"
	@openssl req -new -key $(CA_KEY_PREFIX).key -out $(CA_KEY_PREFIX).csr
	@echo "[+] Unlocking key"
	@cp $(CA_KEY_PREFIX).key $(CA_KEY_PREFIX).key.org
	@openssl rsa -in $(CA_KEY_PREFIX).key.org -out $(CA_KEY_PREFIX).key
	@echo "[+] Auto-signing CSR"
	@openssl x509 -req -days $(CA_KEY_DURATION) -in $(CA_KEY_PREFIX).csr -signkey $(CA_KEY_PREFIX).key -out $(CA_KEY_PREFIX).crt
	@rm -fr $(CA_KEY_PREFIX).key.org $(CA_KEY_PREFIX).csr
	@echo "[+] Created keys and certificate"


clean:
	@rm -fr $(CA_KEY_PREFIX).pem $(CA_KEY_PREFIX).key
	@echo "[+] Removed keys"

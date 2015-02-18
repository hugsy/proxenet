CA_KEY_PREFIX		=	"proxenet"
CA_KEY_SIZE		=	1024
CA_KEY_DURATION		= 	3650
CA_GENERIC_KEY		=	"generic"
CERTS_DIR		=	"certs"

.PHONY :  keys clean

keys:
	@echo "[+] Creating new root Certificate Authority for $(CA_KEY_PREFIX)"
	@echo "[+] Generating new private key"
	@echo blah | openssl genrsa -out $(CA_KEY_PREFIX).key $(CA_KEY_SIZE) >/dev/null 2>&1
	@echo "[+] Creating CSR"
	@openssl req -new -key $(CA_KEY_PREFIX).key -out $(CA_KEY_PREFIX).csr -config ./openssl.cnf -batch >/dev/null 2>&1
	@echo "[+] Unlocking key"
	@cp $(CA_KEY_PREFIX).key $(CA_KEY_PREFIX).key.org
	@openssl rsa -in $(CA_KEY_PREFIX).key.org -out $(CA_KEY_PREFIX).key >/dev/null 2>&1
	@echo "[+] Auto-signing CSR"
	@openssl x509 -req -days $(CA_KEY_DURATION) -in $(CA_KEY_PREFIX).csr -signkey $(CA_KEY_PREFIX).key -out $(CA_KEY_PREFIX).crt >/dev/null 2>&1
	@echo "[+] Cleaning"
	@rm -fr $(CA_KEY_PREFIX).key.org $(CA_KEY_PREFIX).csr
	@echo "[+] Created CA root key and certificate"
	@mkdir -p $(CERTS_DIR)
	@echo blah | openssl genrsa -out $(CERTS_DIR)/$(CA_GENERIC_KEY).key $(CA_KEY_SIZE) >/dev/null 2>&1
	@echo "[+] Created generic key for on-the-fly certificate"
	@echo "[!] Finish the install by importing the certificate \"$(CA_KEY_PREFIX).crt\" in your web browser as \"Trusted Root Authorities\""

clean:
	@rm -f $(CA_KEY_PREFIX).key $(CERTS_DIR)/$(CA_GENERIC_KEY).key
	@echo "[+] Removed keys"
	@rm -f $(CA_KEY_PREFIX).crt $(CERTS_DIR)/*.crt
	@echo "[+] Removed certificates"
	@echo "[!] Remember to clean the certificates imported in your browser too!"

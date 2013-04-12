CA_KEY_PREFIX	=	"proxenet"
CA_KEY_SIZE	=	1024
CA_KEY_DURATION	= 	3650

.PHONY :  all keys

keys:
	@echo "[+] Generating new keys"
	@certtool --generate-privkey --outfile $(CA_KEY_PREFIX).key
	@certtool --generate-self-signed --load-privkey $(CA_KEY_PREFIX).key --outfile $(CA_KEY_PREFIX).pem
	@echo "[+] Created keys";

clean:
	@rm -fr $(CA_KEY_PREFIX).pem $(CA_KEY_PREFIX).key
	@echo "[+] Removed keys"

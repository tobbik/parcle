build:
	cd src && $(MAKE)
	cp src/parcle ./parcle

clean:
	-rm parcle
	cd src && $(MAKE) $@

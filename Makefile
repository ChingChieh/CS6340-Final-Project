.PHONY: clean all

all:
	mkdir -p BUILD && cd BUILD && cmake .. && make -j

clean:
	rm -rf BUILD

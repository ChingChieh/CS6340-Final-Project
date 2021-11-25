FNAME = case1

all:
	clang -S -c -Xclang -disable-O0-optnone -fno-discard-value-names -emit-llvm ${FNAME}.c -o ${FNAME}.ll
	opt -S -mem2reg ${FNAME}.ll -o ${FNAME}.ll

pag:
	wpa -nander -dump-pag ${FNAME}.ll

icfg:
	wpa -type -dump-icfg ${FNAME}.ll

svfg:
	wpa -ander -svfg --dump-vfg -opt-svfg=false ${FNAME}.ll

clean:
	rm ${FNAME}.ll
	rm icfg_final.dot
	rm pag_final.dot
	rm svfg_final.dot

CXX = gcc

all: bradv1 bradv2 pink ploop

bradv1: bradv1_demo.c
	$(CXX) $< -O2 -static -fno-pie -no-pie -fno-reorder-blocks -fno-branch-count-reg -fno-tree-vectorize -lm -o bradv1_demo

bradv2: bradv2_demo.c
	$(CXX) $< -Os -o bradv2_demo

pink: pink_demo.c
	$(CXX) $< -Os -o pink_demo

ploop: ploop_demo.c
	$(CXX) $< -Os -o ploop_demo
	
clean:
	rm -f bradv1_demo
	rm -f bradv2_demo
	rm -f pink_demo
	rm -f ploop_demo

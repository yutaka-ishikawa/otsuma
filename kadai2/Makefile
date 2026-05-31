PROGS = tcp_perf1 tcp_perf2
all: $(PROGS)

tcp_perf1: tcp_perf1.c
	cc -O2 -Wall -o $@ $^
tcp_perf2: tcp_perf2.c
	cc -O2 -Wall -o $@ $^
clean:
	rm -f $(PROGS)

run_server1:
	./tcp_perf1 server any
run_client1:
	./tcp_perf1 client localhost >./result-perf1.csv
run_server2:
	./tcp_perf2 server any
run_client2:
	./tcp_perf2 client localhost >./result-perf2.csv

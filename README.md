## About TDH_Socket_Plugin_for_MySQL

TDH_Socket_Plugin_for_MySQL is a MySQL plugin which like HandlerSocket(https://github.com/DeNADev/HandlerSocket-Plugin-for-MySQL).
And it provide more features and better performance.
It direct access the MySQL handler to bypass sql parse for better performance, and it use thread pool and a series of strategy to have high performance.
	
### Features:
1. Like HandlerSocket with Full Functionality
2. Connection Multiplexing (Dynamic IOStrategy) ,use only one port
3. DDL no hang (can close opened table manually)
4. Support stream output (like dump)
	* Large amount of data ,less memory used
5. Easy to use
	* Execute command without open_table at first(it can be cached with thread)
	* Java Client (https://github.com/alibaba/tdhs-java-client) support JDBC
6. Support multithreading modifying operation
	* A table must be executed in a constant thread
	* One table modifying operation can be configured for concurrency (may cause deadlock)
	* Client can assign thead which to execute the request
7. Dynamic working thread number adjustment (more Physical Reads ,more thread working)
8. Better performance than HandlerSocket when have many Physical Reads scene
9. Can Throttling the Physical Reads

More documents are available in doc/.

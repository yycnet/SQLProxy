SQLProxy数据库集群服务
SQLProxy针对MS SQL数据库集群软件。
SQLProxy允许同时连接n个数据库，进行统一管理。而对于客户端来讲，它看到的只是由SQLProxy表现出来的一个虚拟数据库服务。
客户端只需要连接此SQLProxy虚拟的IP和端口，就能象访问普通数据库那样进行操作。

SQLProxy最大的特色是能够对访问数据库的事务（Transaction）进行并发地处理：当接收到插入、修改、更新等事务操作时，它同时将这个事务（Transaction）发送到后面连接的n台数据库上，这样n台数据库中的数据同时得到了更新；由于在任何时刻，SQLProxy后面连接的n台数据库的数据是完全一致的，因此当接收到查询操作时，整个数据库系统可以实现负载均衡（Load Balance），由此达到客户访问负荷的动态分担，提高整个系统的响应能力。

SQLProxy特性：
数据可靠性和安全性大大增强– 由于任何时刻系统同时拥有多份数据集，因此大大提高了整个系统的数据可靠性和安全性。
服务的可用性大大增强– 如果某一时刻，一台数据库服务器出现问题，其它的数据库服务器仍然能够正常工作；
显著提升数据库系统的性能–在多个独立的数据库系统之间实现动态负载均衡，进而显著提升数据库系统的整体性能。
充分利用已有投资，降低系统TCO（总体拥有成本）–在现有所有别的方案中，备份数据库服务器平时是闲置在那里的，无形中是一种浪费，SQLProxy将这备份数据库服务器也充分利用起来，提高了资源的使用效率，降低了整个数据库系统的TCO。
保证数据库系统具有良好的伸缩性 –通过增加新的数据库服务器即可提升系统的性能、可靠性等。

[2014-07-08] V1.1更新记录：
1、改为独立的线程分析打印SQL语句
2、SQL语句过滤输出支持正则表达式
3、支持服务访问客户端IP限制
4、支持整个数据库实例集群
5、支持后台集群数据库采用不同的访问帐号和密码
6、优化算法，提高集群效率

[2014-07-19]
支持主从负载模式
主从负载模式：  客户端始终连接指定的集群服务器此为主服务器，其它为负载服务器。
		如果当前命令为写入操作且执行成功，则同步到其它负载服务器。
	        如果当前命令为查询操作则随机选择一台负载服务器服务器执行处理，并将负载服务器的返回转发到客户端。
非主从负载模式：客户端随机连接一台集群服务器此为主服务器，其它为负载服务器。
		如果当前命令为写入操作且执行成功，则同步到其它负载服务器。
		如果当前命令为查询操作则直接转发到当前连接的主服务器。

[2014-07-28]
同步失败所有数据库数据回滚

[2014-08-05] V1.2更新记录：
1、支持同步失败所有数据库数据回滚
2、支持采用MSSQL的订阅发布机制进行数据库同步
3、更改集群算法，提高集群效率

[2014-08-15] V1.2更新记录
1、优化并发锁，提高并发查询效率

[2014-12-01]
应使用者要求，Debug模式日志输出不再显示客户机的登录密码
SQLProxy.Net.exe配置数据库界面，数据库登录密码也不再明文显示；保存的信息采用加密方式存储

Q: 如何使用？
A: 初次运行时，先在多台机器上安装要做集群的多个数据库服务器，然后在多台数据库上还原备份数据库。
   初次运行时保证要做集群的数据库服务器数据完全一致。
   然后运行SQLProxy数据库集群软件，添加集群数据库节点(IP以及端口），点击运行即可。
   
   注：原应用系统的数据库连接主机要改变为指向SQLProxy运行所在的机器的IP或机器名，
       因为SQLProxy服务端口和SQL server的默认服务器端口一致为1433，注意避免端口冲突
   
Q：本版本功能是否有限制？
A：功能没人任何限制

Q:连接访问SQLProxy集群时提示" ''拒绝访问 - 访问的数据库和设定的不一致"
A:如果你在SQLProxy界面设置了数据库集群名，则程序默认限制仅仅允许连接指定的数据库，这要求客户端在登录连接数据库时
  必须指定数据库名称，即SQL连接串中"Initial Catalog="必须指定数据库名称和设定的一致，不允许连接数据库后用use指定数据库
  解决办法：1、更改客户端的SQL连接字符串指定数据库名
            2、在SQLProxy界面中不设置数据库集群名

注: 
SQLProxy.Net.exe为vb.net写的界面程序，需要.net Framework2.0及以上支持。
打印日志以及SQL输出为影响SQLProxy集群的效率，因此在正式运行时最好将日志输出设为Error，仅仅输出错误信息同时关闭SQL的打印



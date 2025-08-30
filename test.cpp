#include<iostream>
#include<chrono>
#include<thread>
#include "ThreadPool.h"

/*
如何构建一个Any类型
任意的其它类型 template
能让一个类型指向其它任意的类型？
基类可以指向派生类
Any => Base* ---> Derive:public Base
*/

class MyTask : public Task{
public:
    MyTask(int begin, int end)
    :begin_(begin),end_(end)
    {}
    //如何设计任务返回值，使得对于任何类型的返回值都能适用
    Any run() {
        std::cout << "tid : " << std::this_thread::get_id() << "begin!" <<std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1 ));
        int sum = 0;
        for(int i = begin_; i < end_; i++){
            sum += i;
        }
        std::cout << "tid : " << std::this_thread::get_id() << "end!" <<std::endl;
        return sum;
    }
private:
        int begin_;
        int end_;
};

int main() {
    {
        ThreadPool pool;
        pool.setMode(PoolMode::MODE_CACHED);
        pool.start(2);
        //Linux上，Result对象也是局部对象，需要进行析构
        Result res1 = pool.submitTask(std::make_shared<MyTask>(1,10000));
        Result res2 = pool.submitTask(std::make_shared<MyTask>(10001,20000));
        Result res3 = pool.submitTask(std::make_shared<MyTask>(20001,30000));
        Result res4 = pool.submitTask(std::make_shared<MyTask>(20001,30000));
        Result res5 = pool.submitTask(std::make_shared<MyTask>(20001,30000));
        Result res6 = pool.submitTask(std::make_shared<MyTask>(20001,30000));
        // int sum1 = res1.get().cast_<int>();//返回了一个Any类型，如何转换为具体类型
        // std::cout << sum1 << std::endl;
    }//这里Result对象也要析构 在vs下，条件变量析构会释放相应资源
    std::cout << "main over" << std::endl;
    getchar();

    // {
    //     //问题：ThreadPool对象析构以后，如何把线程池相关的线程资源全部进行回收
    //     ThreadPool pool;
    //     //用户自己设置线程池的工作模式
    //     pool.setMode(PoolMode::MODE_CACHED);
    //     //启动线程池
    //     pool.start(1);
    //     //如何设计这里的Result机制来对返回值进行接收
    //     Result res1 = pool.submitTask(std::make_shared<MyTask>(1,10000));
    //     Result res2 = pool.submitTask(std::make_shared<MyTask>(10001,20000));
    //     Result res3 = pool.submitTask(std::make_shared<MyTask>(20001,30000));
    //     Result res4 = pool.submitTask(std::make_shared<MyTask>(20001,30000));
    //     Result res5 = pool.submitTask(std::make_shared<MyTask>(20001,30000));
    //     Result res6 = pool.submitTask(std::make_shared<MyTask>(20001,30000));
    //     int sum1 = res1.get().cast_<int>();//返回了一个Any类型，如何转换为具体类型
    //     int sum2 = res2.get().cast_<int>();
    //     int sum3 = res3.get().cast_<int>();
    //     int sum4 = res4.get().cast_<int>();
    //     int sum5 = res5.get().cast_<int>();
    //     int sum6 = res6.get().cast_<int>();
    //     std::cout << sum1 + sum2 + sum3 + sum4 + sum5 + sum6 << std::endl;
    // }
     //getchar();


}

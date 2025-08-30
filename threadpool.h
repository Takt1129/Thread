//
// Created by wcm on 2025/8/26.
//

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <thread>
#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unordered_map>
#include <iostream>
class Semaphore {
public:
    Semaphore(int limit = 0)
    : resLimit_(limit) {}
    ~Semaphore() = default;
    //获取一个信号量资源
    void wait() {
        std::unique_lock<std::mutex> lock(mtx_);
        //等待信号量有资源，没有资源会阻塞当前线程
        cond_.wait(lock,[this]()->bool{return resLimit_ > 0;});
        resLimit_--;
    }
    //增加一个信号量资源
    void post() {
        std::unique_lock<std::mutex> lock(mtx_);
        resLimit_++;
        //linux下condition_variable的析构函数什么也没做
        //导致这里状态已经失效，无故阻塞
        cond_.notify_all();
    }
private:
    int resLimit_;
    std::mutex mtx_;
    std::condition_variable cond_;
};
//Any类型：可以接受任意数据的类型
class Any
{
public:
    Any(){};
    ~Any(){};
    Any(const Any&) = delete;
    Any& operator=(const Any&) = delete;
    Any(Any&&) = default;
    Any& operator=(Any&&) = default;
    template<typename T>
    //接受任意类型的数据
    Any(T data) : base_(std::make_unique<Derived<T>>(data))
    {}
    template<typename T>
    T cast_()
    {
        //从Base中找到它所指向的派生类对象，从它里面取出data成员变量
        //基类转派生类 RTTI
        Derived<T> *pd = dynamic_cast<Derived<T>*>(base_.get());
        if(pd == nullptr)
        {
            throw "type is unmatch";
        }
        return pd->data_;
    }
private:
    //基类
    class Base
    {
        public:
            virtual ~Base() = default;
    };
    template<class T>
    class Derived: public Base
    {
    public:
        Derived(T data) : data_(data) {}
        T data_;
    };
    std::unique_ptr<Base> base_;
};
class Task;
class Result
{
public:
    Result(std::shared_ptr<Task> task,bool isvalid = true);
    ~Result() = default;
    //setVal 获取任务执行完的返回值
    void setVal(Any any);
    //get方法 用户调用获取task的返回值
    Any get();
private:
    Any any_;//存储任务的返回值
    Semaphore sem_;//线程通信的信号量
    std::shared_ptr<Task> task_;//指向对应获取返回值的任务对象
    std::atomic_bool isValid_;//返回值是否有效（任务如果提交失败，返回值就是无效的）
};
//任务抽象基类
//
class Task
{
public:
    Task();
    ~Task() = default;
    void setResult(Result* result);
    void exec();
    //用户可以自定义任意任务类型，从Task继承，重写run方法，实现自定义任务处理
    virtual Any run() = 0;
private:
    Result* result_;
};
//线程池支持的模式
enum class PoolMode
{
    MODE_FIXED,//固定数量的线程
    MODE_CACHED,//线程数量可动态增长
};
//线程类型
class Thread
{
public:
    //线程函数对象类型
    using ThreadFunc = std::function<void(int)>;
    //线程构造
    Thread(ThreadFunc func);
    //线程析构
    ~Thread();
    //启动线程
    void start();
    int getId_() const;
private:
    ThreadFunc func_;
    static int generatedId_;
    int threadId_;//保存线程ID
};
class ThreadPool
{
    //需要一个容器存放线程
public:
    ThreadPool();

    ~ThreadPool();

    void start(int initThreadSize = std::thread::hardware_concurrency());//开启线程池

    //设置线程池cached模式下线程阈值
    void setThreadSizeThreshHold(int threadNum);

    void setMode(PoolMode mode);//设置模式

    //设置任务队列上限度的阈值
    void setTaskQueMaxThreshHole(int threshhold);

    Result submitTask(std::shared_ptr<Task> sp);//为线程池提交任务

    //禁止拷贝
    ThreadPool(const ThreadPool&) = delete;

    ThreadPool& operator=(const ThreadPool&) = delete;
private:
    //定义线程函数
    void threadFunc(int threadid);
    bool checkRunningState() const;
private:
    //std::vector<std::unique_ptr<Thread>> threads_;
    std::unordered_map<int,std::unique_ptr<Thread>> threads_;//线程列表
    size_t initThreadSize_;//初始线程数量
    size_t maxThreadSize_;//线程最大数目
    std::atomic_int idleThreadsize_;//空闲线程数目
    std::atomic_int curThreadSize_;//目前线程数目
    std::condition_variable exitCond_;//等待线程资源全部回收

    std::queue<std::shared_ptr<Task>> taskQue_;//用户可能传入的对象生命周期较短,裸指针会有问题,因此选用智能指针
    std::atomic_int taskSize_;//任务的数目
    int taskQueMaxThreshHold_;//任务队列上限阈值
    int threadSizeThreshHold_;//线程数目的上限阈值
    std::mutex taskQuemutex_;//锁,保证任务队列的线程安全
    std::condition_variable notFull;//队列不满
    std::condition_variable notEmpty;//队列不空

    PoolMode poolMode_;//当前线程池的工作模式
    std::atomic_bool isPoolRunning_; //表示线程池是否启动
};



#endif

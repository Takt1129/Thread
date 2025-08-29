//
// Created by wcm on 2025/8/26.
//

#include "threadpool.h"

const int TASK_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_THRESHHOLD = 10;
const int THREAD_MAX_IDLE_TIME = 10;
//线程池构造
ThreadPool::ThreadPool()
    : initThreadSize_(0)
    ,taskSize_(0)
    ,taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
    ,poolMode_(PoolMode :: MODE_FIXED)
    ,isPoolRunning_(false)
    ,threadSizeThreshHold_(200)
    ,curThreadSize_(0)
    ,idleThreadsize_(0)
{}

//线程池析构
ThreadPool::~ThreadPool() {
    isPoolRunning_ = false;//线程池关闭
    notEmpty.notify_all();
    //等待线程池所有线程返回/结束
    std::unique_lock<std::mutex> lock(taskQuemutex_);
    exitCond_.wait(lock,[&]()->bool{return threads_.empty();});
}
//开启线程池
void ThreadPool::start(int initThreadSize)
{
    //设置运行状态
    isPoolRunning_ = true;
    //记录初始线程个数
    initThreadSize_ = initThreadSize;
    curThreadSize_ = initThreadSize;
    //创建线程对象
    for (int i = 0; i < initThreadSize; i++) {
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
        int threadId = ptr->getId_();
        threads_.emplace(threadId,std::move(ptr));//因为是unique指针，需要使用move进行移动构造

    }
    //启动所有线程
    for (int i = 0; i < initThreadSize; i++) {
        threads_[i]->start();//需要执行线程函数
        idleThreadsize_++; //启动一个线程就++
    }
}
bool ThreadPool::checkRunningState() const {
    return isPoolRunning_;
}
//线程方法实现////////////////
int Thread::generatedId_ = 0;

int Thread::getId_() const{
    return threadId_;
}
//线程构造
Thread::Thread(ThreadFunc func)
    : func_(func)
    , threadId_(generatedId_++)
{}
//线程析构
Thread::~Thread() {

}
//线程函数
void ThreadPool::threadFunc(int threadid) {
    auto lastTime = std::chrono::high_resolution_clock().now();
    while (isPoolRunning_){
        std::shared_ptr<Task> task;
        {
        //先获取锁
        std::unique_lock<std::mutex> lock(taskQuemutex_);

        //等待notEmpty条件
        std::cout << "线程" <<std::this_thread::get_id()<< "正在等待取出任务" << std::endl;

        //结束回收掉，超过initThreadSize_数量的线程要进行回收
        //当前时间 - 上一次线程执行的时间 > 60s

            //每一秒钟返回一次线程 怎么区分：超时返回？还是有任务待执行返回
            while (taskQue_.empty()) {
                //条件变量超时返回了
                if (poolMode_ == PoolMode :: MODE_CACHED) {
                if (std::cv_status::timeout == notEmpty.wait_for(lock,std::chrono::seconds(1))){
                    auto now = std::chrono::high_resolution_clock().now();
                    auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
                    if (dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_) {
                        //开始回收当前线程
                        //记录线程相关变量的值修改
                        //将线程对象从线程列表容器中删除 如何精准匹配删除哪个线程
                        //thread_id来寻找线程对象进行删除
                        threads_.erase(threadid);
                        curThreadSize_--;
                        idleThreadsize_--;
                        std::cout << "threadid: " << std::this_thread::get_id()<< "exit!" << std::endl;
                        return ;

                    }
                }
                }else {
                    //等待notEmpty条件
                    notEmpty.wait(lock);
                }
                if (!isPoolRunning_) {
                    threads_.erase(threadid);
                    std::cout << "threadid: " << std::this_thread::get_id()<< "exit!" << std::endl;
                    exitCond_.notify_all();
                    return ;
                }


        }

        //从任务队列中取一个任务出来
        task = taskQue_.front();
        taskQue_.pop();
        taskSize_--;
        idleThreadsize_--;//空闲线程-1
        std::cout << "线程" <<std::this_thread::get_id()<< "已取出任务" << std::endl;

        //如果依然有剩余任务，继续通知其它线程执行任务
        if(taskSize_ > 0) {
            notEmpty.notify_all();
        }

        //取出一个任务，进行通知,可以继续提交生产任务
        notFull.notify_all();
        }
        //释放锁，让别的线程取任务

        //当前线程负责执行这个任务
        if(task != nullptr)
        {
            //task->run();//执行任务，将返回值setVal方法给到Result
            task->exec();

        }
        idleThreadsize_++;
        lastTime = std::chrono::high_resolution_clock().now();//更新线程执行完任务的时间

    }
    threads_.erase(threadid);
    std::cout << "threadid: " << std::this_thread::get_id()<< "exit!" << std::endl;//回收执行完任务的线程
    exitCond_.notify_all();

}
//启动线程
void Thread::start() {
    //创建一个线程来执行线程函数
    std::thread t(func_,threadId_);//C++11来说，线程对象t和线程函数func_
    t.detach();//设置分离线程，这样线程函数就不会结束
}
void ThreadPool::setMode(PoolMode mode)//设置模式
{
    if (checkRunningState() == false)
    poolMode_ = mode;
}
//设置任务队列上限度的阈值
void ThreadPool::setTaskQueMaxThreshHole(int threshhold)
{
    taskQueMaxThreshHold_ = threshhold;
}
//为线程池提交任务
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
    //获取锁
    std::unique_lock<std::mutex> lock(taskQuemutex_);
    //用户提交任务，最长不能阻塞超过一秒，否则判断提交任务失败，返回
    //线程通信 等待任务队列有空余 wait wait_for wait_until
   if(!notFull.wait_for(lock,std::chrono::seconds(1),
        [&]()->bool{ return taskQue_.size() < (size_t)taskQueMaxThreshHold_; }))
        {
        //表示notFull等待1s，条件仍未满足
            std::cerr << "task queue full,submit task fail" << std::endl;
            return Result(sp,false); //Task Result
        }
   //wait(lock, pred) 会在内部反复调用 pred()（持有同一把互斥量 lock），直到返回 true。
   //pred 里要读取共享状态（如 taskQue.size()、taskQueMaxThreshHold_）。
   //如果把这些变量按值捕获，那么在创建 lambda 的那一刻就被拷走了：
   //阈值若之后被修改，pred 看不到最新值；
   //队列若被按值捕获，会产生一份拷贝（既昂贵又错误），始终读的是那份拷贝的 size()。
   //按引用捕获保证 pred 每次都读到受同一把锁保护的真实对象，与其它线程对队列的更新一致。
   //不过——如果 taskQue 和 taskQueMaxThreshHold_ 是类的成员（典型的线程池写法），最小且更清晰的写法应当是捕获 this（即可通过 this 访问成员），而不是用泛化的 [&]：

    //如果有空余，将任务放入队列
    taskQue_.emplace(sp);
    taskSize_++;
    //因为新放了任务，队列肯定不空了，在notempty上进行通知
    notEmpty.notify_all();

    //cached模式 场景：小而快的任务 需要根据任务数目和空闲线程的数目判断是否要创建一个新的线程
    if (poolMode_ == PoolMode :: MODE_CACHED
        && taskSize_ > idleThreadsize_
        && curThreadSize_ < threadSizeThreshHold_)
    {
        //创建新线程
        std::cout << ">>> new thread created <<<" << std::endl;
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
        int threadId = ptr->getId_();
        threads_.emplace(threadId,std::move(ptr));//因为是unique指针，需要使用move进行移动构造
        threads_[threadId]->start();//启动线程
        curThreadSize_++;//增加当前线程
        idleThreadsize_++;//空闲线程
    }

    return Result(sp,true);//task->gerResult()不能使用。线程执行完task，task就被析构掉了，得到返回值的时候task已经被析构了，依赖于task的Result对象也没了
}
void ThreadPool::setThreadSizeThreshHold(int threadNum) {
    if (checkRunningState() == false)
        return;
    if (poolMode_ == PoolMode :: MODE_CACHED)
    threadSizeThreshHold_ = threadNum;
}

Result::Result(std::shared_ptr<Task> task, bool isvalid)
    : isValid_(isvalid), task_(std::move(task)) {
    if (isValid_ && task_) {
        task_->setResult(this); // 关键：建立 Task -> Result 绑定
    }
    }
Any Result::get() {//用户调用的
    if(!isValid_) {
        return " ";
    }
    sem_.wait(); //任务如果没有执行完，这里会阻塞用户的线程
    return std::move(any_);
}
void Result::setVal(Any any) {
    //存储task的返回值
    this->any_ = std::move(any);
    sem_.post();//已经获取任务返回值，增加信号量通知能够返回
}
void Task::exec() {
    if(result_ != nullptr){
    std::cout << "线程" <<std::this_thread::get_id()<< "正在执行任务" << std::endl;
    result_->setVal(std::move(run()));
    }
}
void Task::setResult(Result* result)
{
    result_ = result;
}
// threadpool.cpp


Task::Task() : result_(nullptr) {}

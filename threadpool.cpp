//
// Created by wcm on 2025/8/26.
//

#include "threadpool.h"

const int TASK_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_THRESHHOLD = 10;
const int THREAD_MAX_IDLE_TIME = 10;
//�̳߳ع���
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

//�̳߳�����
ThreadPool::~ThreadPool() {
    isPoolRunning_ = false;//�̳߳عر�
    notEmpty.notify_all();
    //�ȴ��̳߳������̷߳���/����
    std::unique_lock<std::mutex> lock(taskQuemutex_);
    exitCond_.wait(lock,[&]()->bool{return threads_.empty();});
}
//�����̳߳�
void ThreadPool::start(int initThreadSize)
{
    //��������״̬
    isPoolRunning_ = true;
    //��¼��ʼ�̸߳���
    initThreadSize_ = initThreadSize;
    curThreadSize_ = initThreadSize;
    //�����̶߳���
    for (int i = 0; i < initThreadSize; i++) {
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
        int threadId = ptr->getId_();
        threads_.emplace(threadId,std::move(ptr));//��Ϊ��uniqueָ�룬��Ҫʹ��move�����ƶ�����

    }
    //���������߳�
    for (int i = 0; i < initThreadSize; i++) {
        threads_[i]->start();//��Ҫִ���̺߳���
        idleThreadsize_++; //����һ���߳̾�++
    }
}
bool ThreadPool::checkRunningState() const {
    return isPoolRunning_;
}
//�̷߳���ʵ��////////////////
int Thread::generatedId_ = 0;

int Thread::getId_() const{
    return threadId_;
}
//�̹߳���
Thread::Thread(ThreadFunc func)
    : func_(func)
    , threadId_(generatedId_++)
{}
//�߳�����
Thread::~Thread() {

}
//�̺߳���
void ThreadPool::threadFunc(int threadid) {
    auto lastTime = std::chrono::high_resolution_clock().now();
    while (isPoolRunning_){
        std::shared_ptr<Task> task;
        {
        //�Ȼ�ȡ��
        std::unique_lock<std::mutex> lock(taskQuemutex_);

        //�ȴ�notEmpty����
        std::cout << "�߳�" <<std::this_thread::get_id()<< "���ڵȴ�ȡ������" << std::endl;

        //�������յ�������initThreadSize_�������߳�Ҫ���л���
        //��ǰʱ�� - ��һ���߳�ִ�е�ʱ�� > 60s

            //ÿһ���ӷ���һ���߳� ��ô���֣���ʱ���أ������������ִ�з���
            while (taskQue_.empty()) {
                //����������ʱ������
                if (poolMode_ == PoolMode :: MODE_CACHED) {
                if (std::cv_status::timeout == notEmpty.wait_for(lock,std::chrono::seconds(1))){
                    auto now = std::chrono::high_resolution_clock().now();
                    auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
                    if (dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_) {
                        //��ʼ���յ�ǰ�߳�
                        //��¼�߳���ر�����ֵ�޸�
                        //���̶߳�����߳��б�������ɾ�� ��ξ�׼ƥ��ɾ���ĸ��߳�
                        //thread_id��Ѱ���̶߳������ɾ��
                        threads_.erase(threadid);
                        curThreadSize_--;
                        idleThreadsize_--;
                        std::cout << "threadid: " << std::this_thread::get_id()<< "exit!" << std::endl;
                        return ;

                    }
                }
                }else {
                    //�ȴ�notEmpty����
                    notEmpty.wait(lock);
                }
                if (!isPoolRunning_) {
                    threads_.erase(threadid);
                    std::cout << "threadid: " << std::this_thread::get_id()<< "exit!" << std::endl;
                    exitCond_.notify_all();
                    return ;
                }


        }

        //�����������ȡһ���������
        task = taskQue_.front();
        taskQue_.pop();
        taskSize_--;
        idleThreadsize_--;//�����߳�-1
        std::cout << "�߳�" <<std::this_thread::get_id()<< "��ȡ������" << std::endl;

        //�����Ȼ��ʣ�����񣬼���֪ͨ�����߳�ִ������
        if(taskSize_ > 0) {
            notEmpty.notify_all();
        }

        //ȡ��һ�����񣬽���֪ͨ,���Լ����ύ��������
        notFull.notify_all();
        }
        //�ͷ������ñ���߳�ȡ����

        //��ǰ�̸߳���ִ���������
        if(task != nullptr)
        {
            //task->run();//ִ�����񣬽�����ֵsetVal��������Result
            task->exec();

        }
        idleThreadsize_++;
        lastTime = std::chrono::high_resolution_clock().now();//�����߳�ִ���������ʱ��

    }
    threads_.erase(threadid);
    std::cout << "threadid: " << std::this_thread::get_id()<< "exit!" << std::endl;//����ִ����������߳�
    exitCond_.notify_all();

}
//�����߳�
void Thread::start() {
    //����һ���߳���ִ���̺߳���
    std::thread t(func_,threadId_);//C++11��˵���̶߳���t���̺߳���func_
    t.detach();//���÷����̣߳������̺߳����Ͳ������
}
void ThreadPool::setMode(PoolMode mode)//����ģʽ
{
    if (checkRunningState() == false)
    poolMode_ = mode;
}
//��������������޶ȵ���ֵ
void ThreadPool::setTaskQueMaxThreshHole(int threshhold)
{
    taskQueMaxThreshHold_ = threshhold;
}
//Ϊ�̳߳��ύ����
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
    //��ȡ��
    std::unique_lock<std::mutex> lock(taskQuemutex_);
    //�û��ύ�����������������һ�룬�����ж��ύ����ʧ�ܣ�����
    //�߳�ͨ�� �ȴ���������п��� wait wait_for wait_until
   if(!notFull.wait_for(lock,std::chrono::seconds(1),
        [&]()->bool{ return taskQue_.size() < (size_t)taskQueMaxThreshHold_; }))
        {
        //��ʾnotFull�ȴ�1s��������δ����
            std::cerr << "task queue full,submit task fail" << std::endl;
            return Result(sp,false); //Task Result
        }
   //wait(lock, pred) �����ڲ��������� pred()������ͬһ�ѻ����� lock����ֱ������ true��
   //pred ��Ҫ��ȡ����״̬���� taskQue.size()��taskQueMaxThreshHold_����
   //�������Щ������ֵ������ô�ڴ��� lambda ����һ�̾ͱ������ˣ�
   //��ֵ��֮���޸ģ�pred ����������ֵ��
   //����������ֵ���񣬻����һ�ݿ������Ȱ����ִ��󣩣�ʼ�ն������Ƿݿ����� size()��
   //�����ò���֤ pred ÿ�ζ�������ͬһ������������ʵ�����������̶߳Զ��еĸ���һ�¡�
   //����������� taskQue �� taskQueMaxThreshHold_ ����ĳ�Ա�����͵��̳߳�д��������С�Ҹ�������д��Ӧ���ǲ��� this������ͨ�� this ���ʳ�Ա�����������÷����� [&]��

    //����п��࣬������������
    taskQue_.emplace(sp);
    taskSize_++;
    //��Ϊ�·������񣬶��п϶������ˣ���notempty�Ͻ���֪ͨ
    notEmpty.notify_all();

    //cachedģʽ ������С��������� ��Ҫ����������Ŀ�Ϳ����̵߳���Ŀ�ж��Ƿ�Ҫ����һ���µ��߳�
    if (poolMode_ == PoolMode :: MODE_CACHED
        && taskSize_ > idleThreadsize_
        && curThreadSize_ < threadSizeThreshHold_)
    {
        //�������߳�
        std::cout << ">>> new thread created <<<" << std::endl;
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
        int threadId = ptr->getId_();
        threads_.emplace(threadId,std::move(ptr));//��Ϊ��uniqueָ�룬��Ҫʹ��move�����ƶ�����
        threads_[threadId]->start();//�����߳�
        curThreadSize_++;//���ӵ�ǰ�߳�
        idleThreadsize_++;//�����߳�
    }

    return Result(sp,true);//task->gerResult()����ʹ�á��߳�ִ����task��task�ͱ��������ˣ��õ�����ֵ��ʱ��task�Ѿ��������ˣ�������task��Result����Ҳû��
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
        task_->setResult(this); // �ؼ������� Task -> Result ��
    }
    }
Any Result::get() {//�û����õ�
    if(!isValid_) {
        return " ";
    }
    sem_.wait(); //�������û��ִ���꣬����������û����߳�
    return std::move(any_);
}
void Result::setVal(Any any) {
    //�洢task�ķ���ֵ
    this->any_ = std::move(any);
    sem_.post();//�Ѿ���ȡ���񷵻�ֵ�������ź���֪ͨ�ܹ�����
}
void Task::exec() {
    if(result_ != nullptr){
    std::cout << "�߳�" <<std::this_thread::get_id()<< "����ִ������" << std::endl;
    result_->setVal(std::move(run()));
    }
}
void Task::setResult(Result* result)
{
    result_ = result;
}
// threadpool.cpp


Task::Task() : result_(nullptr) {}

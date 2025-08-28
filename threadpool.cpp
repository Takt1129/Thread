//
// Created by wcm on 2025/8/26.
//

#include "threadpool.h"

const int TASK_MAX_THRESHHOLD = 1024;

//�̳߳ع���
ThreadPool::ThreadPool()
    : initThreadSize_(4)
    ,taskSize_(0)
    ,taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
    ,poolMode_(PoolMode :: MODE_FIXED)
{}

//�̳߳�����
ThreadPool::~ThreadPool()
{}
//�����̳߳�
void ThreadPool::start(int initThreadSize)
{
    //��¼��ʼ�̸߳���
    initThreadSize_ = initThreadSize;
    //�����̶߳���
    for (int i = 0; i < initThreadSize; i++) {
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this));
        threads_.emplace_back(std::move(ptr));//��Ϊ��uniqueָ�룬��Ҫʹ��move�����ƶ�����
    }
    //���������߳�
    for (int i = 0; i < initThreadSize; i++) {
        threads_[i]->start();//��Ҫִ���̺߳���
    }
}
//�̷߳���ʵ��
//�̹߳���
Thread::Thread(ThreadFunc func)
    : func_(func)
{}
//�߳�����
Thread::~Thread() {

}
//�̺߳���
void ThreadPool::threadFunc() {
    //std::cout << "begin threadfunc tid ��" << std::this_thread::get_id() << std::endl;
    //std::cout << "end threadfunc tid :" << std::this_thread::get_id() << std::endl;
    for(; ;){
        std::shared_ptr<Task> task;
        {
        //�Ȼ�ȡ��
        std::unique_lock<std::mutex> lock(taskQuemutex_);
        //�ȴ�notEmpty����
        std::cout << "�߳�" <<std::this_thread::get_id()<< "���ڵȴ�ȡ������" << std::endl;
        notEmpty.wait(lock, [&]()->bool {return !taskQue_.empty();});
        //�����������ȡһ���������
        task = taskQue_.front();
        taskQue_.pop();
        taskSize_--;
        std::cout << "�߳�" <<std::this_thread::get_id()<< "��ȡ������" << std::endl;
        //�����Ȼ��ʣ�����񣬼���֪ͨ�����߳�ִ������
        if(taskSize_ > 0) {
            notEmpty.notify_all();
        }
        //ȡ��һ�����񣬽���֪ͨ,���Լ����ύ��������
        notFull.notify_all();
        }//�ͷ������ñ���߳�ȡ����
        //��ǰ�̸߳���ִ���������
        if(task != nullptr)
        {
            //task->run();//ִ�����񣬽�����ֵsetVal��������Result
            task->exec();

        }
    }
}
//�����߳�
void Thread::start() {
    //����һ���߳���ִ���̺߳���
    std::thread t(func_);//C++11��˵���̶߳���t���̺߳���func_
    t.detach();//���÷����̣߳������̺߳����Ͳ������
}
void ThreadPool::setMode(PoolMode mode)//����ģʽ
{
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
    return Result(sp,true);//task->gerResult()����ʹ�á��߳�ִ����task��task�ͱ��������ˣ��õ�����ֵ��ʱ��task�Ѿ��������ˣ�������task��Result����Ҳû��
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
    result_->setVal(run());
    }
}
void Task::setResult(Result* result)
{
    result_ = result;
}
// threadpool.cpp


Task::Task() : result_(nullptr) {}

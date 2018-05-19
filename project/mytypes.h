#ifndef TYPE_H
#define TYPE_H
#include <iostream>
#include <cinttypes>
#include <functional>
#include <utility>
#include <fuse.h>

using namespace std;

typedef uint16_t MemNo;
typedef uint8_t BlockPos;   //0..255
typedef pair<MemNo, BlockPos> Location;

const size_t Blocksize = 65536;
const size_t Blocknr = 65536;
const size_t Size = Blocknr * Blocksize;
const MemNo freeNum = 16;   //��һ����������;����

struct Filenode {
    char filename[102];      //102 Bytes
    MemNo content;          //2 Bytes
    Location last;          //4 Bytes
    Location next;          //4 Bytes
    struct stat st;         //144 Bytes
};
const size_t Nodesize = sizeof(Filenode);
const size_t Nodenr = Blocksize / Nodesize;
const size_t Maxnamelen = sizeof(Filenode::filename) - 1;

ostream& operator<<(ostream& o, const Location& b){
    o<<"("<<b.first<<","<<(int)b.second<<")";
    return o;
}

template <typename T, int MOD = Blocknr>
class Queue{
    T *arr;
    uint32_t *head;
    uint32_t *tail;

public:
    Queue(T* add, uint32_t *hadd, uint32_t *tadd):arr(add), head(hadd), tail(tadd) {
        *head = 0;
        *tail = 0;
    }

    bool empty() {return *head == *tail;}

    bool full() {return ((*tail) + 1) %MOD == *head;}

    T front(){
        if(!empty())
            return arr[*head];
        throw("read empty.\n");
    }

    bool pop(){
        if(empty())
            return false;
        *head = ((*head) + 1) % MOD;
        return true;
    }

    bool push(T data){
        if(full())
            return false;
        arr[*tail] = data;
        *tail = ((*tail) + 1) % MOD;
        return true;
    }

    void prt() const{
        cout<<"------queue------"<<endl;
        cout<<"head:"<<*head<<endl;
        cout<<"tail:"<<*tail<<endl;
        uint32_t i = *head;
        uint32_t k =0;
        for(;i!=*tail;i=(i+1) % MOD){
            cout<<arr[i]<<' ';
            k++;
            if(k==10) break;
        }
        cout<<endl;
        cout<<"-----------------"<<endl;
    }

};

/*
 * �����ļ���Ŀ������65536�������֧��131071�������ļ�����̫��ʱ���ļ�����Ч�ʻ����½�
 * Ͱ��СΪʵ����Ҫ��С��Լ2������΢�����ռ任��Ч�ʣ�����O(1)����
 */
const int Hashsize = Blocknr*2 - 1;
class Hashmap{
    MemNo *arrNo;
    BlockPos *arrPos;
    Filenode* (*locationToNode)(Location ln);
public:
    Hashmap(MemNo *arrNoAdd, BlockPos *arrPosAdd, Filenode* (*locationToNode)(Location ln)):arrNo(arrNoAdd),arrPos(arrPosAdd),locationToNode(locationToNode) {}

    Location findLocation(string name){ //ͨ�������ҵ�һ���ڴ�λ��
        hash<string> hashfun;
        int h = hashfun(name) % Hashsize;
        int st = h;
        //cout << "findhash:" << h << endl;
        while(arrNo[h] != 0){   //���ҵ��սڵ�
            Filenode *tmp = locationToNode({arrNo[h], arrPos[h]});
            //cout << tmp->filename << endl;
            if(tmp->filename == name){
                return {arrNo[h], arrPos[h]};
            }
            h = (h+1) % Hashsize;
            if(h == st) break;  //������������
        }
        return {0,0};   //��{0,0}��ʾ����ʧ��
    }

    bool setNewNode(string name, Location ln){  //��һ���������õ�һ���ڴ�λ��
        hash<string> hashfun;
        int h = hashfun(name) % Hashsize;
        int st = h;
        //cout << "sethash:" << h <<endl;
        while(arrNo[h] != 0){   //�ҵ���һ����λ
            Filenode *tmp = locationToNode({arrNo[h], arrPos[h]});
            if(tmp->filename == name){  //�Ѿ��и�������
                return false;
            }
            h = (h+1) % Hashsize;
            if(h == st){    //����
                return false;
            }
        }
        //cout << "pos:" << h <<endl;
        arrNo[h] = ln.first;
        arrPos[h] = ln.second;
        return true;
    }

    bool clearNode(string name){
        hash<string> hashfun;
        int h = hashfun(name) % Hashsize;
        int st = h;
        //cout << "clearhash:" << h << endl;
        while(arrNo[h] != 0){   //���ҵ��սڵ�
            Filenode *tmp = locationToNode({arrNo[h], arrPos[h]});
            //cout << tmp->filename << endl;
            if(tmp->filename == name){
                arrNo[h] = 0;
                arrPos[h] = 0;
                return true;
            }
            h = (h+1) % Hashsize;
            if(h == st) break;  //������������
        }
        return false;   //��{0,0}��ʾ����ʧ��
    }
};

struct BasicInfo{
    Location root;
    size_t blocknr;
    size_t blocksize;
    size_t size;
    uint32_t memhead;
    uint32_t memtail;
    uint32_t infohead;
    uint32_t infotail;
    uint32_t filenum;   //�ļ�����
};


#endif


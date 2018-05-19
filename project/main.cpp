#define FUSE_USE_VERSION 26
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fuse.h>
#include <sys/mman.h>
#include <iostream>
#include <inttypes.h>
#include <functional>
#include <string>
#include <time.h>
#include "mytypes.h"

using namespace std;

static void *mem[Blocknr];

void mapBlock(MemNo k){
    if(k<freeNum){
        cout<<"something wrong!!!!!!"<<endl;
        while(1);
    }
    mem[k] = mmap(NULL, Blocksize , PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    memset(mem[k], 0, Blocksize);
}

void unmapBlock(MemNo k){
    munmap(mem[k], Blocksize);
    mem[k] = NULL;
}

BasicInfo *binfo;
Queue<MemNo> *qmem;
Queue<Location> *ql;
uint8_t *referenceCount;    //引用计数
MemNo *nextBlock;
Hashmap *hashmap;

Filenode *ltoN(Location ln){
    return (Filenode*)((char*)mem[ln.first] + Nodesize*ln.second);
}

void prtNode(Filenode *node){
    cout<<"--------filenode---------"<<endl;
    cout<<node->filename<<endl;
    cout<<"blocks:"<<node->st.st_blocks<<endl;
    MemNo k = node->content;
    int t = 0;
    while(k!=0){
        cout<<k<<" ";
        k = nextBlock[k];
        t++;
        if(t==10)
            break;
    }
    cout<<endl;
    cout<<"-------------------------"<<endl;
}

static void *oshfs_init(struct fuse_conn_info *conn)
{
    //cout<<"*init"<<endl;
    // 分配freeNum个块(连续的)用于储存元信息
    mem[0] = mmap(NULL, Blocksize * freeNum , PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    memset(mem[0], 0, Blocksize * freeNum);
    for(int i=0;i<freeNum;i++){
        mem[i] = (char*)mem[0] + Blocksize*i;
    }

    binfo = (BasicInfo*)mem[0];
    qmem = new Queue<MemNo>((MemNo*)mem[1], &(binfo->memhead), &(binfo->memtail));
    ql = new Queue<Location>((Location*)mem[3], &(binfo->infohead), &(binfo->infotail));
    referenceCount = (uint8_t*)mem[7];
    nextBlock = (MemNo*)mem[8];
    hashmap = new Hashmap((MemNo*)mem[10], (BlockPos*)mem[14], ltoN);

    binfo->blocknr = Blocknr;
    binfo->blocksize = Blocksize;
    binfo->size = Size;
    binfo->filenum = 0;
    binfo->root = Location(0,0);

    for(int i=freeNum; i<(int)Blocknr; i++){
        qmem->push((MemNo)i);
    }
    qmem->prt();

    return NULL;
}

static Filenode *get_filenode(const char *path)
{
    //cout<<"get_filenode:"<<path<<endl;
    Location l = hashmap->findLocation(path+1);
    if(l == Location(0,0))
        return NULL;
    else
        return ltoN(l);
}

static int create_filenode(const char *filename, const struct stat *st)
{
    //cout<<"create_filenode:"<<filename<<endl;

    if(strlen(filename) > Maxnamelen){
        cerr << "Filename too long!" << endl;
        return -ENAMETOOLONG;
    }

    //cout << "name test OK!" << endl;
    //ql->prt();

    while(!ql->empty() && referenceCount[ql->front().first] == 0){   //使用的是已被回收的信息块
        ql->pop();
    }

    cout<<"recycle"<<endl;

    if(ql->empty()){    //当前无可用位置
        if(qmem->empty()){
            cerr << "Not enough storage!" << endl;
            return -ENOSPC;
        }
        MemNo no = qmem->front();
        qmem->pop();
        mapBlock(no);
        for(int i=0;i<(int)Nodenr;i++){
            ql->push(Location(no, (BlockPos)i));
        }
    }

    //cout<<"mem OK!"<<endl;

    Location ln = ql->front();
    //cout<<"Location:"<<ln.first<<","<<(int)ln.second<<endl;
    ql->pop();
    Filenode *newnode = ltoN(ln);
    strcpy(newnode->filename, filename);

    //cout<<"set filename OK!"<<endl;

    memcpy(&(newnode->st), st, sizeof(struct stat));
    newnode -> content = (MemNo)0;
    newnode -> next = binfo -> root;
    newnode -> last = Location(0,0);
    ltoN(binfo -> root) -> last = ln;
    binfo -> root = ln;

    hashmap -> setNewNode(filename, ln);
    referenceCount[ln.first] += 1;

    return 0;
}

static int oshfs_getattr(const char *path, struct stat *stbuf)
{
    //cout<<"*getattr:"<<path<<endl;
    int ret = 0;
    Filenode *node = get_filenode(path);
    if(strcmp(path, "/") == 0) {
        memset(stbuf, 0, sizeof(struct stat));
        stbuf->st_mode = S_IFDIR | 0755;
    } else if(node) {
        memcpy(stbuf, &(node->st), sizeof(struct stat));
    } else {
        ret = -ENOENT;
    }
    return ret;
}

static int oshfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    //cout<<"*readdir"<<endl;
    Location ln = binfo->root;
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    while(ln != Location(0,0)) {
        Filenode *node = ltoN(ln);
        filler(buf, node->filename, &(node->st), 0);
        ln = node->next;
    }
    return 0;
}

static int oshfs_mknod(const char *path, mode_t mode, dev_t dev)
{
    //cout<<"*mknod"<<endl;
    struct stat st;
    st.st_mode = S_IFREG | 0644;
    st.st_uid = fuse_get_context()->uid;
    st.st_gid = fuse_get_context()->gid;
    st.st_nlink = 1;
    st.st_size = 0;
    st.st_blocks = 0;
    st.st_blksize = Blocksize;
    return create_filenode(path + 1, &st);
}

static int oshfs_open(const char *path, struct fuse_file_info *fi)
{
    return 0;
}

static int oshfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    //cout<<"*write:"<<path<<" "<<offset<<" "<<size<<endl;
    Filenode *node = get_filenode(path);

    if(node == NULL){
        return -ENOENT;
    }

    prtNode(node);

    if((size_t)(offset + size) > Blocksize*(node->st.st_blocks)){ //将文件块补够
        if(node->content == 0){  //若是空节点，需要连上第一个块
            if(qmem->empty()){
                cerr << "Not enough storage!" << endl;
                return -ENOSPC;
            }
            MemNo newmem = qmem->front();
            qmem->pop();
            mapBlock(newmem);
            node->content = newmem;
            nextBlock[newmem] = 0;
            node->st.st_blocks = 1;
        }

        int need = ((offset + size - 1) / Blocksize) + 1;   //共需要块数
        MemNo nowmem = node->content;
        while(nextBlock[nowmem] != 0){      //找到当前最后一个块
            nowmem = nextBlock[nowmem];
        }
        //cout<<"lastblocks:"<<nowmem<<endl;
        //cout<<"needblocks:"<<need<<endl;
        //qmem->prt();
        while(node->st.st_blocks < need){   //块数不足,增加新块
            if(qmem->empty()){
                cerr << "Not enough storage!" << endl;
                return -ENOSPC;
            }
            MemNo newmem = qmem->front();
            qmem->pop();
            mapBlock(newmem);
            nextBlock[nowmem] = newmem;
            nextBlock[newmem] = 0;
            node->st.st_blocks += 1;
        }
    }

    node->st.st_size = max((size_t)node->st.st_size, (size_t)offset + size);

    MemNo nowmem = node->content;
    int bn = 0;
    while((bn+1)*Blocksize <= (size_t)offset){  //找到offset所在块
        nowmem = nextBlock[nowmem];
        bn++;
    }
    //cout<<"find offset in block:"<<nowmem<<endl;

    size_t rest = offset - bn*Blocksize;    //不完全的第一个块内剩的空间
    size_t rsize = size;                    //还未写入的大小
    size_t hsize = 0;                       //已经写入的大小

    if(Blocksize - rest >= rsize){
        memcpy((char*)mem[nowmem]+rest, buf, rsize);
        hsize = rsize;
        rsize = 0;
    }
    else{
        memcpy((char*)mem[nowmem]+rest, buf, Blocksize-rest);
        rsize -= Blocksize-rest;
        hsize += Blocksize-rest;
    }

    while(rsize > 0){
        nowmem = nextBlock[nowmem];
        memcpy((char*)mem[nowmem], buf+hsize, min(Blocksize,rsize));
        rsize -= min(Blocksize, rsize);
        hsize += min(Blocksize, rsize);
    }
    //qmem->prt();
    //prtNode(node);
    return size;
}

static int oshfs_unlink(const char *path)
{
    //cout<<"*unlink:"<<path<<endl;
    Location ln = hashmap->findLocation(path+1);
    hashmap->clearNode(path+1);
    Filenode *node = ltoN(ln);
    if(ln == Location(0,0)){
        return -ENOENT;
    }

    MemNo nowmem = node -> content; //释放数据块
    while(nowmem != 0){
        MemNo nextmem = nextBlock[nowmem];
        qmem->push(nowmem);
        unmapBlock(nowmem);
        nextBlock[nowmem] = 0;
        nowmem = nextmem;
    }

    if(ln == binfo->root){      //维护链表
        binfo->root = node->next;
        ltoN(node->next)->last = Location(0,0);
    }
    else{
        ltoN(node->last)->next = node->next;
        ltoN(node->next)->last = node->last;
    }

    //ql->prt();
    //cout<<ln<<endl;
    referenceCount[ln.first] -= 1;
    ql->push(ln);
    if(referenceCount[ln.first] == 0){  //该信息块已空，回收利用,队列中的位置在使用时处理
        qmem->push(ln.first);
        unmapBlock(ln.first);
        nextBlock[ln.first] = 0;
    }

    return 0;
}

static int oshfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    //cout<<"*read"<<endl;
    const Filenode *node = get_filenode(path);

    if(node == NULL){
        return -ENOENT;
    }

    int ret = size;
    size_t rsize = size;    //还需要读取的大小
    if(offset + size > (size_t)node->st.st_size)
        ret = rsize = node->st.st_size - offset;

    MemNo nowmem = node->content;
    int bn = 0;
    while((bn+1)*Blocksize <= (size_t)offset){  //找到offset块
        nowmem = nextBlock[nowmem];
        bn++;
    }

    size_t rest = offset - bn*Blocksize;    //不完全的第一个块内剩的空间
    size_t hsize = 0;                       //已经读取的大小

    if(Blocksize - rest >= rsize){
        memcpy(buf, (char*)mem[nowmem]+rest, rsize);
        hsize = rsize;
        rsize = 0;
    }
    else{
        memcpy(buf, (char*)mem[nowmem]+rest, Blocksize-rest);
        rsize -= Blocksize-rest;
        hsize += Blocksize-rest;
    }

    while(rsize > 0){
        nowmem = nextBlock[nowmem];
        memcpy((char*)mem[nowmem], buf+hsize, min(Blocksize,rsize));
        rsize -= min(Blocksize, rsize);
        hsize += min(Blocksize, rsize);
    }

    return ret;
}

static int oshfs_truncate(const char *path, off_t size)
{
    //cout<<"*truncate:"<<path<<" size:"<<size<<endl;
    Filenode *node = get_filenode(path);

    if(node == NULL){
        return -ENOENT;
    }

    node->st.st_size = size;

    size_t offset = size;   //offset位置后的内容可丢弃
    MemNo nowmem = node->content;
    int bn = 0;
    while((bn+1)*Blocksize < (size_t)offset){  //找到offset块，大小正好一致时，要停在前一块，与read、write不同
        nowmem = nextBlock[nowmem];
        bn++;
    }

    MemNo throwmem = nextBlock[nowmem];
    if(size == 0){
        throwmem = node->content;
        node->content = 0;
    }
    else{
        nextBlock[nowmem] = 0;  //该块之后可以截断
    }

    while(throwmem != 0){
        //cout<<"throw:"<<throwmem<<endl;
        MemNo nextthrow = nextBlock[throwmem];
        unmapBlock(throwmem);
        qmem->push(throwmem);    //回收利用
        nextBlock[throwmem] = 0;
        throwmem = nextthrow;
        node->st.st_blocks -= 1;
    }

    return 0;
}

static const struct fuse_operations op = {  //C++貌似不支持不完全初始化
    oshfs_getattr,          //getattr
    0,                      //readlink
    0,                      //getdir
    oshfs_mknod,            //mknod
    0,                      //mkdir
    oshfs_unlink,           //unlink
    0,                      //rmdir
    0,                      //symlink
    0,                      //rename
    0,                      //link
    0,                      //chmod
    0,                      //chown
    oshfs_truncate,         //truncate
    0,                      //utime
    oshfs_open,             //open
    oshfs_read,             //read
    oshfs_write,            //write
    0,                      //statfs
    0,                      //flush
    0,                      //release
    0,                      //fsync
    0,                      //setxattr
    0,                      //getxattr
    0,                      //listxattr
    0,                      //removexattr
    0,                      //opendir
    oshfs_readdir,          //readdir
    0,                      //releasedir
    0,                      //fsyncdir
    oshfs_init,             //init
    0,                      //destroy
    0,                      //access
    0,                      //create_filenode
    0,                      //ftruncate
    0,                      //fgetattr
    0,                      //lock
    0,                      //utimens
    0,                      //bmap
    0

};

int main(int argc, char *argv[])
{
    //cout<<sizeof(Filenode)<<endl;
    return fuse_main(argc, argv, &op, NULL);
}


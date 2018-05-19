各block的定义：
block 0 ： 整个文件系统的信息，定义为basicinfo
block 1-2：循环队列，记录不同块使用情况，最多储存65535个块编号，每个编号2B
block 3-6：循环队列，记录被分配为文件节点记录的块中，未被使用的位置，每个位置由2B块编号和2B块内位置表示
block 7-10：循环队列，记录分配为文件信息记录的块中，未被使用的位置
block 11-12：线性表，记录对应块（若为数据块）的后继（无后继则为0）
block 13-18：hash表（闭散列），65536×6B，通过文件名hash对应到块及块内位置，每个信息用3B可记录，为对齐方便，将前4块用于记录块编号，后2块用于记录块内位置（由于文件节点每个大小为256B，块内位置共256个，可用1B记录）
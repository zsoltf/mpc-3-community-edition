/* Read-only semantic inventory through libext2fs: no host extraction metadata. */
#include <ext2fs/ext2fs.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static ext2_filsys fs;
static void check(errcode_t e) { if(e){fprintf(stderr,"ext2fs error %ld\n",(long)e);exit(1);} }
static void hex(const void *p,size_t n){const unsigned char *s=p;for(size_t i=0;i<n;i++)printf("%02x",s[i]);}
static int attr(char *name,char *value,size_t length,void *ctx){(void)ctx;printf(" x:");hex(name,strlen(name));putchar('=');hex(value,length);return 0;}
static void visit(ext2_ino_t ino,const char *path);
static int entry(ext2_ino_t dir,int entry_type,struct ext2_dir_entry *d,int offset,int blocksize,char *buf,void *data){
 (void)dir;(void)entry_type;(void)offset;(void)blocksize;(void)buf;
 int n=ext2fs_dirent_name_len(d);if(!d->inode || (n==1 && d->name[0]=='.') || (n==2 && !memcmp(d->name,"..",2)))return 0;
 char *p=malloc(strlen(data)+n+2);if(!p)exit(1);sprintf(p,"%s/%.*s",(char*)data,n,d->name);visit(d->inode,p);free(p);return 0;
}
static void visit(ext2_ino_t ino,const char *path){
 struct ext2_inode i;check(ext2fs_read_inode(fs,ino,&i));hex(path,strlen(path));
 printf(" mode:%o uid:%u gid:%u flags:%u links:%u",i.i_mode,inode_uid(i),inode_gid(i),i.i_flags,i.i_links_count);
 if(LINUX_S_ISREG(i.i_mode) || LINUX_S_ISLNK(i.i_mode)){
  unsigned char digest[32];SHA256_CTX hash;SHA256_Init(&hash);
  if(LINUX_S_ISLNK(i.i_mode) && ext2fs_is_fast_symlink(&i))SHA256_Update(&hash,i.i_block,i.i_size);
  else {ext2_file_t f;check(ext2fs_file_open(fs,ino,0,&f));char buffer[65536];unsigned int n;
   do{check(ext2fs_file_read(f,buffer,sizeof(buffer),&n));SHA256_Update(&hash,buffer,n);}while(n);check(ext2fs_file_close(f));}
  SHA256_Final(digest,&hash);printf(" bytes:%llu sha256:",(unsigned long long)EXT2_I_SIZE(&i));hex(digest,32);
 }else if(!LINUX_S_ISDIR(i.i_mode)) {printf(" device:%u,%u",i.i_block[0],i.i_block[1]);}
 struct ext2_xattr_handle *h;check(ext2fs_xattrs_open(fs,ino,&h));check(ext2fs_xattrs_read(h));check(ext2fs_xattrs_iterate(h,attr,NULL));check(ext2fs_xattrs_close(&h));putchar('\n');
 if(LINUX_S_ISDIR(i.i_mode))check(ext2fs_dir_iterate2(fs,ino,0,NULL,entry,(void*)path));
}
int main(int argc,char **argv){if(argc!=2)return 2;check(ext2fs_open(argv[1],EXT2_FLAG_64BITS,0,0,unix_io_manager,&fs));visit(EXT2_ROOT_INO,"");check(ext2fs_close(fs));return 0;}

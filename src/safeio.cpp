#include "safeio.h"
#include <QDir>
#include <QFile>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <linux/openat2.h>
#include <cerrno>
#include <cstring>

namespace {
struct Descriptor {
 int fd;
 ~Descriptor() { if(fd>=0)::close(fd); }
};
}

extern "C" int quickrefOpenBeneath(int directory, const char *relative, int flags) {
 struct open_how how{};
 how.flags=flags|O_CLOEXEC;
 how.resolve=RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_MAGICLINKS|RESOLVE_NO_XDEV;
 return int(::syscall(SYS_openat2,directory,relative,&how,sizeof(how)));
}

QString SafeIo::systemError() { return QString::fromLocal8Bit(std::strerror(errno)); }

bool SafeIo::publishNew(int directory, const QByteArray &name, const QByteArray &contents, QString &error) {
 // The inode stays unnamed until fully written. A shared-directory writer
 // cannot replace it, and linkat never replaces an occupied destination.
 Descriptor temporary{::openat(directory,".",O_TMPFILE|O_WRONLY|O_CLOEXEC,0600)};
 if(temporary.fd<0){error=systemError();return false;}
 qsizetype offset=0;
 while(offset<contents.size()) {
  const auto count=::write(temporary.fd,contents.constData()+offset,contents.size()-offset);
  if(count<0 && errno==EINTR)continue;
  if(count<=0){if(count==0)errno=EIO;error=systemError();return false;}
  offset+=count;
 }
 if(::fsync(temporary.fd)<0){error=systemError();return false;}
 const auto source=QByteArray("/proc/self/fd/")+QByteArray::number(temporary.fd);
 if(::linkat(AT_FDCWD,source.constData(),directory,name.constData(),AT_SYMLINK_FOLLOW)<0) {
  error=errno==EEXIST ? QStringLiteral("A file with this name already exists; refusing to replace it") : systemError();
  return false;
 }
 return true;
}

bool SafeIo::installSkill(const QString &codexHome, QString &error) {
 // Walk and create directories by descriptor, rejecting symlinks at every
 // component. Never let mkpath or QSaveFile follow a destination symlink.
 const auto absolute=QDir(codexHome).absolutePath();
 Descriptor directory{::open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC)};
 if(directory.fd<0){error=systemError();return false;}
 auto descend=[&](const QString &component, bool privateDirectory) {
  const auto name=QFile::encodeName(component);
  if(::mkdirat(directory.fd,name.constData(),0700)<0 && errno!=EEXIST){error=systemError();return false;}
  const int next=::openat(directory.fd,name.constData(),O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
  if(next<0){error=systemError();return false;}
  struct stat metadata{};
  if(::fstat(next,&metadata)<0){error=systemError();::close(next);return false;}
  if(privateDirectory && (metadata.st_uid!=::geteuid() || (metadata.st_mode & (S_IWGRP|S_IWOTH)))) {
   error="Skill directories must be owned by the current user and not writable by group or others";
   ::close(next);return false;
  }
  ::close(directory.fd);directory.fd=next;return true;
 };
 const auto components=absolute.split('/',Qt::SkipEmptyParts);
 for(int i=0;i<components.size();++i)
  if(!descend(components[i],i==components.size()-1))return false;
 if(!descend("skills",true) || !descend("quickref-viewer",true))return false;
 QFile source(":/agent-skill.md");
 if(!source.open(QIODevice::ReadOnly)){error=source.errorString();return false;}
 return publishNew(directory.fd,"SKILL.md",source.readAll(),error);
}

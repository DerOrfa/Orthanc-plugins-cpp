# Orthanc-plugins-cpp

Plugins for [Orthanc](https://www.orthanc-server.com/).
Those are heavily tailored to my needs. So don't come at me if they don't work for you (which they probably won't).
Feel free to steal them, though..

## shadowwriter.cpp
Uses [OrthancPluginRegisterStorageArea](https://sdk.orthanc-server.com/group__Callbacks.html#ga1a63a48ff8db57b8d7e0238bdf8d487c) to "replace the built-in way Orthanc stores its files on the filesystem".

It stores files just like Orthanc in "StorageDirectory" *and* creates hardlinks to those files populating a directory given by "ShadowPath" with a structure of the form:
[PatientName]/[StudyDate][StudyTime]/S[SequenceNumber]_[SequenceDescription]/[InstanceUID]`.

If "StorageDirectory" and "ShadowPath" are not on the same device, it will fall back to creating symlinks.

The created links are removed, if the original file is removed via Orthanc. Empty directories are removed too.
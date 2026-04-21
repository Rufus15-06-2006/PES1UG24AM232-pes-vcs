# PES-VCS — Version Control System Implementation
### Name: Piyush Shiv
### SRN: PES1UG24AM191

## Project Overview
This project is a custom Version Control System (VCS) built from scratch in C, modeled after the internal architecture of Git. It implements content-addressable storage, directory tree snapshots, a staging area (index), and a persistent commit history.

---

## Architecture & OS Concepts
* **Content-Addressable Storage:** Files are stored based on their SHA-256 hash rather than filenames.
* **Atomic Writes:** Implementation of the "temp-file-then-rename" pattern to ensure filesystem integrity.
* **Directory Sharding:** Sharding the `.pes/objects` directory to optimize lookups.
* **Metadata Tracking:** Utilizing `stat` to track `mtime` and `size` for fast change detection.

---

## Phases of Development & Evidence

### Phase 1: Object Storage
**Screenshot 1A (Tests):**
![1A](./screenshots/1a.png)
*Output showing all tests passing for blob storage and deduplication.*

**Screenshot 1B (Sharding):**
![1B](./screenshots/1b.png)
*Directory structure showing sharded objects under `.pes/objects/XX/`.*

---

### Phase 2: Tree Objects
**Screenshot 2A (Tests):**
![2A](./screenshots/2a.png)
*Successful serialization and parsing of tree hierarchies.*

**Screenshot 2B (Binary View):**
![2B](./screenshots/2b.png)
*Hex dump using `xxd` showing the binary structure of a Tree object.*

---

### Phase 3: The Index (Staging Area)
**Screenshot 3A (Status):**
![3A](./screenshots/3a.png)
*Command output of `./pes status` showing staged files.*

**Screenshot 3B (Index File):**
![3B](./screenshots/3b.png)
*Raw content of `.pes/index` showing metadata and hashes.*

---

### Phase 4: Commits and History
**Screenshot 4A (Commit Log):**
![4A](./screenshots/4a.png)
*Verification of commit creation and linked history.*

**Screenshot 4B (Object Growth):**
![4B](./screenshots/4b.png)
*Object store growth after multiple commits.*

**Screenshot 4C (References):**
![4C](./screenshots/4c.png)
*Current HEAD pointing to the latest commit hash.*

**Screenshot 4 Test (Testing):**
![4test](./screenshots/4test.png)
*Full test integration result.*

---

## Analysis Questions

### Section 5: Branching and Checkout
**Q5.1 Implementation of `checkout`:** To implement `pes checkout <branch>`, I would update the `HEAD` file to point to the new branch reference. The working directory must then be updated by reading the Tree object associated with the target commit. This is complex because it requires a safe diff: deleting files not in the target tree, adding missing ones, and updating modified ones without losing uncommitted user data.

**Q5.2 Dirty Directory Detection:** I would compare the current working directory's `mtime` and `size` (via `stat`) against the entries in the `index`. If they differ, the directory is "dirty." To check against the branch, I would compare the `index` hashes against the hashes in the target commit's Tree object.

**Q5.3 Detached HEAD:** If a commit is made in a detached HEAD state, the new commit's parent is the previous hash, but no branch reference (like `main`) is updated. To recover these, a user would need to find the orphaned commit hash in the object store and manually point a new branch at it.

### Section 6: Garbage Collection (GC)
**Q6.1 Reachability Algorithm:** I would use a **Mark-and-Sweep** algorithm. 
1. **Mark:** Start from all references in `refs/heads/` and `HEAD`, recursively following every tree and blob hash.
2. **Sweep:** Iterate through all files in `.pes/objects/` and delete any file whose name (hash) is not in the "reachable" set.

**Q6.2 Race Conditions in GC:** If GC runs while a `commit` is happening, GC might see a newly written blob that isn't *yet* referenced by a commit object and delete it. Git avoids this using a grace period (not deleting recent objects).

---

## How to Run
```bash
make pes
./pes init
./pes add <filename>
./pes commit -m "Your message"
./pes log

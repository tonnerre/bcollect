/*
 * (c) 2007, Tonnerre Lombard <tonnerre@bsdprojects.net>,
 *	     BSD projects network. All rights reserved.
 *
 * Redistribution and use in source  and binary forms, with or without
 * modification, are permitted  provided that the following conditions
 * are met:
 *
 * * Redistributions of  source code  must retain the  above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this  list of conditions and the  following disclaimer in
 *   the  documentation  and/or  other  materials  provided  with  the
 *   distribution.
 * * Neither the name of the BSD  projects network nor the name of its
 *   contributors may  be used to endorse or  promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS"  AND ANY EXPRESS  OR IMPLIED WARRANTIES  OF MERCHANTABILITY
 * AND FITNESS  FOR A PARTICULAR  PURPOSE ARE DISCLAIMED. IN  NO EVENT
 * SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL,  EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED  TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE,  DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT  LIABILITY,  OR  TORT  (INCLUDING NEGLIGENCE  OR  OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <bcollect.h>

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif

void
do_backup(struct interval *interval, struct backup *backup)
{
	time_t now_tt = time(NULL);
	struct tm *now = localtime(&now_tt);

	size_t proglen = strlen(backup->dest) + 21;
	size_t locklen = strlen(backup->dest) + 7;
	size_t nbackups = interval->count + 1;
	size_t len = strlen(backup->dest) + strlen(interval->name) +
		strlen(backup->name) + 20;
	pid_t pid;

	char *lockpath = malloc(locklen);
	char *progpath = malloc(proglen);
	char *latestpath = NULL;
	char *path = malloc(len + 1);

	int lockfd, progfd;


	if (!path || !lockpath)
	{
		perror("malloc");
		goto out_free;
	}

	memset(lockpath, 0, locklen);
	memset(path, 0, len);

	snprintf(lockpath, locklen, "%s/.lock", backup->dest);
	snprintf(progpath, proglen, "%s/.backup_in_progress", backup->dest);
	snprintf(path, len - 16, "%s/%s-%s-", backup->dest, backup->name,
		interval->name);
	strftime(path + len - 17, 16, "%Y-%m-%d_%Hh%M", now);

	lockfd = open(lockpath, O_WRONLY | O_CREAT,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	if (lockfd == -1)
	{
		fprintf(stderr, "Locking backup directory for %s: %s\n",
			backup->name, strerror(errno));
		goto out_free;
	}
	if (flock(lockfd, LOCK_EX | LOCK_NB))
	{
		if (errno == EAGAIN)
			fprintf(stderr, "%s: Backup already in progress\n",
				backup->name);
		else
			fprintf(stderr, "Locking backup directory for %s: %s\n",
				backup->name, strerror(errno));

		goto out_close_lock;
	}

	progfd = open(progpath, O_WRONLY | O_CREAT | O_EXCL,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	if (progfd == -1)
	{
		/**
		 * Backup in progress marker still around, we should
		 * delete one backup.
		 */
		struct dirent *de;
		struct stat sb;
		char *latest;
		time_t latestmtime = 0;
		DIR *d;

		if (errno != EEXIST)
		{
			fprintf(stderr, "Locking backup directory for %s: %s\n",
				backup->name, strerror(errno));
			goto out_unlock;
		}
		if (!(d = opendir(backup->dest)))
		{
			perror("opendir");
			close(progfd);
			goto out_unlock;
		}
		while (de = readdir(d))
		{
			size_t entry_len = strlen(backup->dest) +
				strlen(de->d_name) + 3;
			size_t minlen = MIN(strlen(de->d_name),
				strlen(backup->name));
			char *entry = NULL;

			/* Check if the directory entry is a backup */
			if (minlen < strlen(backup->name) ||
				strncmp(de->d_name, backup->name, minlen))
				continue;

			entry = malloc(entry_len);

			if (!entry)
			{
				perror("malloc");
				continue;
			}

			memset(entry, 0, entry_len);
			snprintf(entry, entry_len - 1, "%s/%s", backup->dest,
				de->d_name);

			if (stat(entry, &sb)) perror(entry);
			else if (sb.st_mtime > latestmtime)
			{
				if (latest) free(latest);
				latest = entry;
				latestmtime = sb.st_mtime;
			}
			else
				free(entry);
		}
		closedir(d);

		if (latest)
		{
			fprintf(stderr, "Removing stale backup directory %s\n",
				latest);
			rmdir_recursive(latest);
			free(latest);
		}
	}
	close(progfd);

	while (nbackups > interval->count)
	{
		DIR *dirp;
		char *oldest = NULL;
		struct dirent *de;
		struct stat sb;
		time_t oldest_time = 0, latest_time = 0;

		nbackups = 0;
		memset(&sb, 0, sizeof(struct stat));

		if (!(dirp = opendir(backup->dest)))
		{
			perror("opendir");
			goto out_unlock;
		}
		while (de = readdir(dirp))
		{
			size_t entry_len = strlen(backup->dest) +
				strlen(de->d_name) + 3;
			size_t minlen = MIN(strlen(de->d_name),
				strlen(backup->name));
			char *entry = NULL;

			/* Check if the directory entry is a backup */
			if (minlen < strlen(backup->name) ||
				strncmp(de->d_name, backup->name, minlen))
				continue;

			nbackups++;
			entry = malloc(entry_len);
			if (!entry)
			{
				perror("malloc");
				continue;
			}

			if (entry == oldest)
				fputs("WARNING: malloc did something weird!\n",
					stderr);

			memset(entry, 0, entry_len);
			snprintf(entry, entry_len - 1, "%s/%s", backup->dest,
				de->d_name);

			if (stat(entry, &sb)) perror(entry);
			else if (sb.st_mtime > latest_time)
			{
				if (latestpath) free(latestpath);
				latestpath = strdup(entry);
				latest_time = sb.st_mtime;

				if (!oldest_time || sb.st_mtime < oldest_time)
				{
					if (oldest) free(oldest);
					oldest = strdup(entry);
					oldest_time = sb.st_mtime;
				}
			}
			else if (!oldest_time || sb.st_mtime < oldest_time)
			{
				if (oldest) free(oldest);
				oldest = strdup(entry);
				oldest_time = sb.st_mtime;
			}
			free(entry);
		}
		closedir(dirp);

		if (oldest && nbackups > interval->count)
		{
			fprintf(stderr, "Removing old backup %s\n", oldest);
			rmdir_recursive(oldest);
		}

		if (oldest) free(oldest);
	}

	/*
	 * We can mkdir only now because we weren't protected by the lock
	 * before.
	 */
	if (mkdir(path, 0755))
	{
		perror(path);
		goto out_unlock;
	}

	/*
	 * Call rsync and let it fill realpath with data from source.
	 */
	pid = fork();

	if (pid < 0)
	{
		perror("fork");
		goto out_unlock;
	}
	else if (!pid)
	{
		struct exclude *pos;
		char **argv;
		int argc = 9;
		int argi;

		for (pos = backup->excludelist.next; pos &&
			pos->next != &backup->excludelist; pos = pos->next)
			argc += 2;

		if (latestpath)
			argc += 2;

		argv = malloc(argc * sizeof(char *));

		argv[0] = RSYNC_PATH;
		argv[1] = "-a";
		argv[2] = "--delete";
		argv[3] = "--numeric-ids";
		argv[4] = "--relative";
		argv[5] = "--delete-excluded";

		argi = 6;

		if (latestpath)
		{
			argv[argi++] = "--link-dest",
			argv[argi++] = latestpath;
		}

		for (pos = backup->excludelist.next; pos &&
			pos->next != &backup->excludelist; pos = pos->next)
		{
			argv[argi++] = "--exclude";
			argv[argi++] = pos->pattern;
		}

		argv[argi++] = backup->source;
		argv[argi++] = path;
		argv[argi++] = NULL;

		execv(RSYNC_PATH, argv);
		perror(RSYNC_PATH);
	}
	else
	{
		int exitcode = 0;

		if (waitpid(pid, &exitcode, 0) < 0)
		{
			perror("wait");
			rmdir_recursive(path);
			goto out_unlock;
		}

		if (WEXITSTATUS(exitcode))
		{
			fprintf(stderr, "rsync process exited with code %d!\n",
				WEXITSTATUS(exitcode));
			rmdir_recursive(path);
			goto out_unlock;
		}
	}

out_unlock:
	flock(lockfd, LOCK_UN);

out_close_lock:
	close(lockfd);
	unlink(progpath);
	unlink(lockpath);

out_free:
	if (latestpath) free(latestpath);
	if (progpath) free(progpath);
	if (lockpath) free(lockpath);
	if (path) free(path);
}

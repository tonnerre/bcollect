/*
 * (c) 2007, Caoimhe Chaos <caoimhechaos@protonmail.com>,
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

struct c_hashtable *intervals;

struct backup backups;
struct backup *current_backup = NULL;

void
init_interval(void)
{
	intervals = c_hashtable_new(c_stringhash, c_stringequals);

	backups.next = backups.prev = &backups;
}

void
declare_interval(char *name, int count)
{
	struct interval *iv = malloc(sizeof(struct interval));

	if (!iv)
	{
		perror("malloc");
		return;
	}

	iv->name = name;
	iv->count = count;

	if (!c_hashtable_replace(intervals, name, iv))
	{
		perror("c_hashtable_replace");
		free(iv);
		return;
	}
}

void
backup_add(void)
{
	current_backup = malloc(sizeof(struct backup));
	if (!current_backup)
	{
		perror("malloc");
		exit(EXIT_FAILURE);
	}

	memset(current_backup, 0, sizeof(struct backup));
	current_backup->excludelist.next = &current_backup->excludelist;
	current_backup->excludelist.prev = &current_backup->excludelist;
}

void
backup_name(char *name)
{
	if (!current_backup)
	{
		fprintf(stderr, "No backup selected! (This shouldn't "
			"happen)\n");
		exit(EXIT_FAILURE);
	}

	current_backup->name = name;
}

void
backup_source(char *source)
{
	if (!current_backup)
	{
		fprintf(stderr, "No backup selected! (This shouldn't "
			"happen)\n");
		exit(EXIT_FAILURE);
	}

	current_backup->source = source;
}

void
backup_dest(char *dest)
{
	if (!current_backup)
	{
		fprintf(stderr, "No backup selected! (This shouldn't "
			"happen)\n");
		exit(EXIT_FAILURE);
	}

	current_backup->dest = dest;
}

void
backup_summary(unsigned long flag)
{
	if (!current_backup)
	{
		fprintf(stderr, "No backup selected! (This shouldn't "
			"happen)\n");
		exit(EXIT_FAILURE);
	}

	current_backup->summary = !!flag;
}

/* FIXME: some of these functions should be unified */
void
backup_fromcc(unsigned long flag)
{
	if (!current_backup)
	{
		fprintf(stderr, "No backup selected! (This shouldn't "
			"happen)\n");
		exit(EXIT_FAILURE);
	}

	current_backup->ccollect = !!flag;
}

void
backup_manual(unsigned long flag)
{
	if (!current_backup)
	{
		fprintf(stderr, "No backup selected! (This shouldn't "
			"happen)\n");
		exit(EXIT_FAILURE);
	}

	current_backup->manualonly = !!flag;
}

void
backup_exclude(char *pattern)
{
	struct exclude *entry;

	if (!current_backup)
	{
		fprintf(stderr, "No backup selected! (This shouldn't "
			"happen)\n");
		exit(EXIT_FAILURE);
	}

	entry = malloc(sizeof(struct exclude));
	if (!entry)
	{
		perror("malloc");
		exit(EXIT_FAILURE);
	}

	memset(entry, 0, sizeof(struct exclude));
	entry->pattern = pattern;

	entry->next = &current_backup->excludelist;
	entry->prev = current_backup->excludelist.prev;
	current_backup->excludelist.prev->next = entry;
	current_backup->excludelist.prev = entry;
}

void
backup_preexec(char *cmdline)
{
	if (!current_backup)
	{
		fprintf(stderr, "No backup selected! (This shouldn't "
			"happen)\n");
		exit(EXIT_FAILURE);
	}

	/* XXX: Unquote! But since we really don't support quoting yet... */
	current_backup->preexec = cmdline;
}

void
backup_postexec(char *cmdline)
{
	if (!current_backup)
	{
		fprintf(stderr, "No backup selected! (This shouldn't "
			"happen)\n");
		exit(EXIT_FAILURE);
	}

	/* XXX: Unquote! But since we really don't support quoting yet... */
	current_backup->postexec = cmdline;
}

void
backup_finalize(void)
{
	if (!current_backup)
	{
		fprintf(stderr, "No backup selected! (This shouldn't "
			"happen)\n");
		exit(EXIT_FAILURE);
	}

	if (!current_backup->name)
	{
		fprintf(stderr, "Ignoring backup: no name defined\n");
		free(current_backup);
		current_backup = NULL;
		return;
	}

	if (!current_backup->source || !current_backup->dest)
	{
		fprintf(stderr, "Ignoring backup %s: no source or "
			"destination defined\n", current_backup->name);
		if (current_backup->source)
			free(current_backup->source);
		if (current_backup->dest)
			free(current_backup->dest);
		if (current_backup->preexec)
			free(current_backup->preexec);
		if (current_backup->postexec)
			free(current_backup->postexec);
		free(current_backup->name);
		free(current_backup);
		current_backup = NULL;
		return;
	}

	backups.prev->next = current_backup;
	current_backup->prev = backups.prev;
	backups.prev = current_backup;
	current_backup->next = &backups;
	current_backup = NULL;
}

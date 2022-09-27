/* Kernel Programming */
#define LINUX

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <asm/io.h>

#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>

#include <linux/spinlock_types.h>
#include <linux/delay.h>

/******************************************************************************/
/* Constants								      */
/******************************************************************************/
#define MAX_CPUS							      40
#define MAX_CLOCK_STR_LENGTH						      32
#define MAX_SAMPLE_ENTRIES						10000000
#define INITIAL_CPU_ARRAY						       \
		{ -1, -1, -1, -1, -1, -1, -1, -1,			       \
		  -1, -1, -1, -1, -1, -1, -1, -1,			       \
		  -1, -1, -1, -1, -1, -1, -1, -1,			       \
		  -1, -1, -1, -1, -1, -1, -1, -1,			       \
		  -1, -1, -1, -1, -1, -1, -1, -1 }

/******************************************************************************/
/* Entries								      */
/******************************************************************************/
enum {
	WAKEUP_PARAS_SAMPLE = 0,
	CPUS_ALLOWED_SAMPLE,
};

typedef struct sample_entry {
	unsigned long long sched_clock;
	char entry_type;
	union {
		struct {
			unsigned char cur_cpu, on_rq;
            int parent_pid;
		} wakeup_paras_sample;
        struct {
            int * cpus_in_mask;
        } cpus_allowed_sample;
		
	} data;
} sample_entry_t;

sample_entry_t *sample_entries = NULL;
unsigned long current_sample_entry_id = 0;
unsigned long n_sample_entries = 0;

/******************************************************************************/
/* Hook function types							      */
/******************************************************************************/
typedef void (*record_wakeup_paras_t)(unsigned int, int, int);
typedef void (*record_cpus_allowed_t)(cpumask_t *, int);

/******************************************************************************/
/* Prototypes								      */
/******************************************************************************/
extern int * sp_parse_cpumask(cpumask_t * m, int n_cpu);
extern void set_sp_module_record_wakeup_paras
	(record_wakeup_paras_t __sp_module_record_wakeup_paras);
extern void set_sp_module_record_cpus_allowed
	(record_cpus_allowed_t __sp_module_record_cpus_allowed);

static void rq_stats_do_work(struct seq_file *m, unsigned long iteration);

/******************************************************************************/
/* Functions								      */
/******************************************************************************/
static void *my_seq_start(struct seq_file *s, loff_t *pos)
{
	static unsigned long iteration = 0;
	if (*pos == 0 || *pos < n_sample_entries) {
		return &iteration;
	} else {
		*pos = 0;
		return NULL;
	}
}

static void *my_seq_next(struct seq_file *s, void *v, loff_t *pos)
{

	unsigned long *iteration = (unsigned long *)v;

	(*iteration)++;
	(*pos)++;

	if (*iteration < n_sample_entries)
		return v;
	else
		return NULL;
}

static void my_seq_stop(struct seq_file *s, void *v) {}

static int my_seq_show(struct seq_file *s, void *v)
{
	rq_stats_do_work(s, *(unsigned long *)v);
	return 0;
}

static struct seq_operations my_seq_ops = {
	.start = my_seq_start,
	.next  = my_seq_next,
	.stop  = my_seq_stop,
	.show  = my_seq_show
};

static int rq_stats_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &my_seq_ops);
}

static const struct file_operations rq_stats_fops = {
	.owner   = THIS_MODULE,
	.open	= rq_stats_open,
	.read	= seq_read,
	.llseek  = seq_lseek,
	.release = seq_release,
};

static void rq_stats_do_work(struct seq_file *m, unsigned long iteration)
{
	int i;
	sample_entry_t *entry;
	set_sp_module_record_wakeup_paras(NULL);
	set_sp_module_record_cpus_allowed(NULL);

	n_sample_entries = current_sample_entry_id < MAX_SAMPLE_ENTRIES ?
			   current_sample_entry_id : MAX_SAMPLE_ENTRIES;

	entry = &sample_entries[iteration];

	if (entry->entry_type == WAKEUP_PARAS_SAMPLE)
	{
        seq_printf(m, " WAKEUP       %4d %4d %12d\n", 
                        entry->data.wakeup_paras_sample.cur_cpu,
                        entry->data.wakeup_paras_sample.on_rq,
                        entry->data.wakeup_paras_sample.parent_pid);
	}
	if (entry->entry_type == CPUS_ALLOWED_SAMPLE)
	{	seq_printf(m, " ALLOWED_CPUS ");
		for(i = 0; i < 64; i++) {
            if (entry->data.cpus_allowed_sample.cpus_in_mask[i] == -1) break;
            seq_printf(m, "%4d, ", entry->data.cpus_allowed_sample.cpus_in_mask[i]);
        }
		seq_printf(m, "\n");
	}
}

//void sched_profiler_set_nr_running(unsigned long int *nr_running_p,
//				     unsigned long int new_nr_running,
//				     int dst_cpu)
void sched_profiler_record_wakeup_paras(unsigned int cur_cpu, int on_rq, int parent_pid)
{
	unsigned long __current_sample_entry_id;

	__current_sample_entry_id =
		__sync_fetch_and_add(&current_sample_entry_id, 1);

	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		set_sp_module_record_wakeup_paras(NULL);
        set_sp_module_record_cpus_allowed(NULL);
		return;
	}

	sample_entries[__current_sample_entry_id]
		.sched_clock = ktime_get_mono_fast_ns();
	sample_entries[__current_sample_entry_id]
		.entry_type = WAKEUP_PARAS_SAMPLE;
	sample_entries[__current_sample_entry_id].data.wakeup_paras_sample
		.cur_cpu = (unsigned char)cur_cpu;
	sample_entries[__current_sample_entry_id].data.wakeup_paras_sample
		.on_rq = (unsigned char)on_rq;
    sample_entries[__current_sample_entry_id].data.wakeup_paras_sample
		.parent_pid = parent_pid;
}

void sched_profiler_record_cpus_allowed(cpumask_t * m, int n_cpu) {
	unsigned long __current_sample_entry_id;

	__current_sample_entry_id =
		__sync_fetch_and_add(&current_sample_entry_id, 1);

	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		set_sp_module_record_wakeup_paras(NULL);
        set_sp_module_record_cpus_allowed(NULL);
		return;
	}

	sample_entries[__current_sample_entry_id]
		 .sched_clock = ktime_get_mono_fast_ns();
	sample_entries[__current_sample_entry_id]
		 .entry_type = CPUS_ALLOWED_SAMPLE;
	sample_entries[__current_sample_entry_id].data.cpus_allowed_sample
		 .cpus_in_mask = sp_parse_cpumask(m, n_cpu);
}

void sched_profiler_forbid_task_cpu(struct task_struct * p, int cpu) {
	cpumask_t newmask;
	cpumask_copy(&newmask, &p->cpus_allowed);
	cpumask_bits(&newmask)[cpu] = 0;
	set_cpus_allowed_ptr(p, &newmask);
}
int init_module(void)
{
	unsigned long sample_entries_size =
		sizeof(sample_entry_t) * MAX_SAMPLE_ENTRIES;

	proc_create("sched_profiler", S_IRUGO, NULL, &rq_stats_fops);

	if(!(sample_entries = vmalloc(sample_entries_size)))
	{
		printk("sched_profiler: kmalloc fail (%lu bytes)\n",
			   sample_entries_size);
		remove_proc_entry("sched_profile", NULL);

		return -ENOMEM;
	}

	set_sp_module_record_wakeup_paras(sched_profiler_record_wakeup_paras);
	set_sp_module_record_cpus_allowed(sched_profiler_record_cpus_allowed);

	return 0;
}

void cleanup_module(void)
{
	set_sp_module_record_wakeup_paras(NULL);
	set_sp_module_record_cpus_allowed(NULL);

	ssleep(1);

	vfree(sample_entries);
	remove_proc_entry("sched_profiler", NULL);
}

MODULE_LICENSE("GPL");

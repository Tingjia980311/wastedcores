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
#define MAX_CPUS							      64
#define MAX_CLOCK_STR_LENGTH						      32
#define MAX_SAMPLE_ENTRIES						10000000
#define INITIAL_CPU_ARRAY						       \
		{ -1, -1, -1, -1, -1, -1, -1, -1,			       \
		  -1, -1, -1, -1, -1, -1, -1, -1,			       \
		  -1, -1, -1, -1, -1, -1, -1, -1,			       \
		  -1, -1, -1, -1, -1, -1, -1, -1,			       \
		  -1, -1, -1, -1, -1, -1, -1, -1,			       \
		  -1, -1, -1, -1, -1, -1, -1, -1,			       \
		  -1, -1, -1, -1, -1, -1, -1, -1,			       \
		  -1, -1, -1, -1, -1, -1, -1, -1 }

enum {
	SP_SCHED_EXEC = 0,
	SP_TRY_TO_WAKE_UP,
	SP_WAKE_UP_NEW_TASK,
	SP_IDLE_BALANCE,
	SP_REBALANCE_DOMAINS,
	SP_MOVE_TASKS = 10,
	SP_ACTIVE_LOAD_BALANCE_CPU_STOP = 20,
	SP_CONSIDERED_CORES_SIS = 200,
	SP_CONSIDERED_CORES_USLS,
	SP_CONSIDERED_CORES_FBQ,
	SP_CONSIDERED_CORES_FIG,
	SP_CONSIDERED_CORES_FIC
};

/******************************************************************************/
/* Kernel declarations							      */
/******************************************************************************/
struct rq {
	raw_spinlock_t lock;
	unsigned int nr_running;
#ifdef CONFIG_NUMA_BALANCING
	unsigned int nr_numa_running;
	unsigned int nr_preferred_running;
#endif
#define CPU_LOAD_IDX_MAX 5
	unsigned long cpu_load[CPU_LOAD_IDX_MAX];
	unsigned long last_load_update_tick;
#ifdef CONFIG_NO_HZ_COMMON
	u64 nohz_stamp;
	unsigned long nohz_flags;
#endif
#ifdef CONFIG_NO_HZ_FULL
	unsigned long last_sched_tick;
#endif
	int skip_clock_update;
	struct load_weight load;
   /* ...goes on. */
};

/******************************************************************************/
/* Entries								      */
/******************************************************************************/
enum {
	RQ_SIZE_SAMPLE = 0,
	SCHEDULING_SAMPLE,
	SCHEDULING_SAMPLE_EXTRA,
	BALANCING_SAMPLE,
	LOAD_SAMPLE
};

typedef struct sample_entry {
	unsigned long long sched_clock;
	char entry_type;
	union {
		struct {
			unsigned char nr_running, dst_cpu;
		} rq_size_sample;
		struct {
			unsigned char event_type, src_cpu, dst_cpu;
		} scheduling_sample;
#ifdef WITH_SCHEDULING_SAMPLE_EXTRA
		struct {
			unsigned char event_type, data1, data2, data3, data4,
				      data5, data6, data7, data8;
		} scheduling_sample_extra;
#endif
		struct {
			unsigned char event_type, cpu, current_cpu;
			uint64_t data;
		} balancing_sample;
		struct {
			unsigned long load;
			unsigned char cpu;
		} load_sample;
	} data;
} sample_entry_t;

sample_entry_t *sample_entries = NULL;
unsigned long current_sample_entry_id = 0;
unsigned long n_sample_entries = 0;

/******************************************************************************/
/* Hook function types							      */
/******************************************************************************/
//typedef void (*set_nr_running_t)(unsigned long int*, unsigned long int, int);
typedef void (*set_nr_running_t)(int*, int, int);
typedef void (*record_scheduling_event_t)(int, int, int);
#ifdef WITH_SCHEDULING_SAMPLE_EXTRA
typedef void (*record_scheduling_event_extra_t)(int, char, char, char, char,
						char, char, char, char);
#endif
typedef void (*record_balancing_event_t)(int, int, uint64_t);
typedef void (*record_load_change_t)(unsigned long, int);

/******************************************************************************/
/* Prototypes								      */
/******************************************************************************/
extern struct rq *sp_cpu_rq(int cpu);
extern void set_sp_module_set_nr_running
	(set_nr_running_t __sp_module_set_nr_running);
extern void set_sp_module_record_load_change
	(record_load_change_t __sp_module_record_load_change);

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
	static unsigned int rq_size[MAX_CPUS] = INITIAL_CPU_ARRAY;
	static unsigned long load[MAX_CPUS] = INITIAL_CPU_ARRAY;

	set_sp_module_set_nr_running(NULL);
	set_sp_module_record_load_change(NULL);

	n_sample_entries = current_sample_entry_id < MAX_SAMPLE_ENTRIES ?
			   current_sample_entry_id : MAX_SAMPLE_ENTRIES;

	entry = &sample_entries[iteration];


	if (entry->entry_type == RQ_SIZE_SAMPLE)
	{
		rq_size[entry->data.rq_size_sample.dst_cpu] =
			entry->data.rq_size_sample.nr_running;
	}
	else if (entry->entry_type == LOAD_SAMPLE)
	{
		load[entry->data.load_sample.cpu] =
			entry->data.load_sample.load;
	}

	for (i = 0; i < MAX_CPUS; i++)
		seq_printf(m, "%4d", rq_size[i]);

	seq_printf(m, "	 %llu nsecs", entry->sched_clock);

	if (entry->entry_type == RQ_SIZE_SAMPLE)
	{
		seq_printf(m, " RQ %4d %4d", entry->data.rq_size_sample.dst_cpu,
			   entry->data.rq_size_sample.nr_running);
	}
	else if (entry->entry_type == LOAD_SAMPLE)
	{
		seq_printf(m, " LD %4d %12ld", entry->data.load_sample.cpu,
			   entry->data.load_sample.load);
	}

	seq_printf(m, "\n");

	/* TODO: Rewrite using this format to improve performance? */

//	seq_printf(m, "%u %u\n", rq_size_sample_entries[iteration].nr_running,
//		   rq_size_sample_entries[iteration].dst_cpu);
}

//void sched_profiler_set_nr_running(unsigned long int *nr_running_p,
//				     unsigned long int new_nr_running,
//				     int dst_cpu)
void sched_profiler_set_nr_running(int *nr_running_p, int new_nr_running,
				   int dst_cpu)
{
	unsigned long __current_sample_entry_id;

	*nr_running_p = new_nr_running;

	__current_sample_entry_id =
		__sync_fetch_and_add(&current_sample_entry_id, 1);

	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		set_sp_module_set_nr_running(NULL);
		set_sp_module_record_load_change(NULL);
		return;
	}

	sample_entries[__current_sample_entry_id]
		.sched_clock = ktime_get_mono_fast_ns();
	sample_entries[__current_sample_entry_id]
		.entry_type = RQ_SIZE_SAMPLE;
	sample_entries[__current_sample_entry_id].data.rq_size_sample
		.nr_running = (unsigned char)new_nr_running;
	sample_entries[__current_sample_entry_id].data.rq_size_sample
		.dst_cpu = (unsigned char)dst_cpu;
}


void sched_profiler_record_load_change(unsigned long load, int cpu)
{
	unsigned long __current_sample_entry_id;

	__current_sample_entry_id =
		__sync_fetch_and_add(&current_sample_entry_id, 1);

	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		set_sp_module_set_nr_running(NULL);
		set_sp_module_record_load_change(NULL);
		return;
	}

	sample_entries[__current_sample_entry_id]
		 .entry_type = LOAD_SAMPLE;

	sample_entries[__current_sample_entry_id]
		 .sched_clock = ktime_get_mono_fast_ns();
	sample_entries[__current_sample_entry_id]
		 .entry_type = LOAD_SAMPLE;
	sample_entries[__current_sample_entry_id].data.load_sample
		 .load = load;
	sample_entries[__current_sample_entry_id].data.load_sample
		 .cpu = (unsigned char)cpu;
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

	set_sp_module_set_nr_running(sched_profiler_set_nr_running);
	set_sp_module_record_load_change(NULL);

	return 0;
}

void cleanup_module(void)
{
	set_sp_module_set_nr_running(NULL);
	set_sp_module_record_load_change(NULL);

	ssleep(1);

	vfree(sample_entries);
	remove_proc_entry("sched_profiler", NULL);
}

MODULE_LICENSE("GPL");

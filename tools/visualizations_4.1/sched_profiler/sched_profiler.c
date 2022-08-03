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
	LOAD_SAMPLE,
	LOAD_BALANCE_SAMPLE,
	FILE_WRITING_SAMPLE,
	CPUALLOWED_SAMPLE,
	REBALANCE_SAMPLE,
	DOMAINCPU_SAMPLE,
	IDLE_BALANCE_SAMPLE,
	HOTCAHCE_SAMPLE,
	DOMAIN_FLAG_SAMPLE,
	DOMIAN_INTERVAL_SAMPLE,
	CGROUP_CPUMASK_SAMPLE,
	START_MIGRATION_SAMPLE,
	END_MIGRATION_SAMPLE,
	BUSIEST_SAMPLE,
	DETACH_STATUS_SAMPLE,
	DETACH_LOAD_SAMPLE
};

typedef struct sample_entry {
	unsigned long long sched_clock;
	char entry_type;
	union {
		struct {
			unsigned char nr_running, dst_cpu;
		} rq_size_sample;
		struct {
			int flag;
			unsigned long load;
			unsigned char cpu;
		} load_sample;
		struct {
			unsigned char src_cpu, dst_cpu, ld_moved;
		} load_balance_sample;
		struct {
		} file_writing_sample;
		struct {
			unsigned char cpu;
		} cpu_allowed_sample;
		struct {
			unsigned char cpu, level, r;
		} rebalance_sample;
		struct {
			unsigned char cpu;
		} domain_sample;
		struct {
			unsigned char src_cpu, dst_cpu;
		} hotcache_sample;
		struct {
			unsigned char cpu, pulled_task;
		} idle_balance_sample;
		struct {
			unsigned char cpu, level, flag;
		} domain_flag_sample;
		struct {
			unsigned char cpu, level;
			unsigned int interval;
		} domain_interval_sample;
		struct {
			unsigned char cpu;
		} cgroup_cpumask_sample;
		struct {
			unsigned char src_cpu, dst_cpu;
		} start_migration_sample;
		struct {
			unsigned char src_cpu, dst_cpu, ld_moved;
		} end_migration_sample;
		struct {
			int flag;
			unsigned char cpu;
		} busiest_sample;
		struct {
			unsigned char cpu;
			unsigned int loop, loop_max, loop_break;
			long imbalance;
		} detach_status_sample;
		struct {
			unsigned long load;
		} detach_load_sample;
	} data;
} sample_entry_t;

sample_entry_t *sample_entries = NULL;
unsigned long current_sample_entry_id = 0;
unsigned long n_sample_entries = 0;

/******************************************************************************/
/* Hook function types							      */
/******************************************************************************/
typedef void (*set_nr_running_t)(int*, int, int);
typedef void (*record_load_change_t)(int, unsigned long, int);
typedef void (*record_load_balance_t)(int, int, int);
typedef void (*record_file_writing_t)(void);
typedef void (*record_cpuallowed_change_t)(int);
typedef void (*record_rebalance_t)(int,int,int);
typedef void (*record_sd_cpus_t)(int);
typedef void (*record_hotcache_reject_t)(int,int);
typedef void (*record_idle_balance_t)(int, int);
typedef void (*record_sd_flag_t)(int, int, int);
typedef void (*record_sd_interval_t)(int, int, unsigned int);
typedef void (*record_cgroup_cpumask_t)(int);
typedef void (*record_start_migration_t)(int, int);
typedef void (*record_end_migration_t)(int, int, int);
typedef void (*record_busiest_t)(int, int);
typedef void (*record_detach_status_t)(int, unsigned int, unsigned int, unsigned int, long);
typedef void (*record_detach_load_t)(unsigned long);


/******************************************************************************/
/* Prototypes								      */
/******************************************************************************/
extern struct rq *sp_cpu_rq(int cpu);
extern void set_sp_module_set_nr_running
	(set_nr_running_t __sp_module_set_nr_running);
extern void set_sp_module_record_load_change
	(record_load_change_t __sp_module_record_load_change);
extern void set_sp_module_record_load_balance
	(record_load_balance_t __sp_module_record_load_balance);
extern void set_sp_module_record_file_writing
	(record_file_writing_t __sp_module_record_file_writing);
extern void set_sp_module_record_cpuallowed_change
	(record_cpuallowed_change_t __sp_module_record_cpuallowed_change);
extern void set_sp_module_record_rebalance
	(record_rebalance_t __sp_module_record_rebalance);
extern void set_sp_module_record_sd_cpus
	(record_sd_cpus_t __sp_module_record_sd_cpus);
extern void set_sp_module_record_hotcache_rej
	(record_hotcache_reject_t __sp_module_record_hotcahe_rej);
extern void set_sp_module_record_idle_balance
	(record_idle_balance_t __sp_module_record_idle_balance);
extern void set_sp_module_record_sd_flag
	(record_sd_flag_t __sp_module_record_sd_flag);
extern void set_sp_module_record_sd_interval
	(record_sd_interval_t __sp_module_record_sd_interval);
extern void set_sp_module_record_cgroup_cpumask
	(record_cgroup_cpumask_t __sp_module_record_cgroup_cpumask);
extern void set_sp_module_record_start_migration
	(record_start_migration_t __sp_module_record_start_migration);
extern void set_sp_module_record_end_migration
	(record_end_migration_t __sp_module_record_end_migration);
extern void set_sp_module_record_busiest
	(record_busiest_t __sp_module_record_busiest);
extern void set_sp_module_record_detach_status
	(record_detach_status_t __sp_module_record_detach_status);
extern void set_sp_module_record_detach_load
	(record_detach_load_t __sp_module_record_detach_load);

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
	static unsigned long avg_cfs_load[MAX_CPUS] = INITIAL_CPU_ARRAY;

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
		if (entry->data.load_sample.flag == 0)
			load[entry->data.load_sample.cpu] =
				entry->data.load_sample.load;
		if (entry->data.load_sample.flag == 1)
			avg_cfs_load[entry->data.load_sample.cpu] = 
				entry->data.load_sample.load;
	}

	if (entry->entry_type == RQ_SIZE_SAMPLE)
	{
		for (i = 0; i < MAX_CPUS; i++)
			seq_printf(m, "%4d", rq_size[i]);
	}
	if (entry->entry_type == LOAD_SAMPLE)
	{	
		if (entry->data.load_sample.flag == 0){
			for (i = 0; i < MAX_CPUS; i++)
				seq_printf(m, "%12ld", load[i]);
		} else if (entry->data.load_sample.flag == 1) {
			for (i = 0; i < MAX_CPUS; i++)
				seq_printf(m, "%12ld", avg_cfs_load[i]);
		}
		
	}

	seq_printf(m, "	 %llu nsecs", entry->sched_clock);

	if (entry->entry_type == RQ_SIZE_SAMPLE)
	{		
		seq_printf(m, " RQ %4d %4d", entry->data.rq_size_sample.dst_cpu,
			   entry->data.rq_size_sample.nr_running);
	}
	else if (entry->entry_type == LOAD_SAMPLE)
	{
		seq_printf(m, " LD %4d %4d %12ld", entry->data.load_sample.flag, entry->data.load_sample.cpu,
			   entry->data.load_sample.load);
	}
	else if (entry->entry_type == LOAD_BALANCE_SAMPLE)
	{
		seq_printf(m, " LB %4d %4d %4d", entry->data.load_balance_sample.src_cpu,
			   entry->data.load_balance_sample.dst_cpu, 
			   entry->data.load_balance_sample.ld_moved);
	}
	else if (entry->entry_type == FILE_WRITING_SAMPLE)
	{
		seq_printf(m, " WF");
	}
	else if (entry->entry_type == CPUALLOWED_SAMPLE)
	{
		seq_printf(m, " CC %4d", entry->data.cpu_allowed_sample.cpu);
	}
	else if (entry->entry_type == REBALANCE_SAMPLE)
	{
		seq_printf(m, " RB %4d %4d %4d", entry->data.rebalance_sample.cpu, 
				entry->data.rebalance_sample.level, entry->data.rebalance_sample.r);
	}
	else if (entry->entry_type == DOMAINCPU_SAMPLE)
	{
		seq_printf(m, " DOMAIN_RB %4d", entry->data.domain_sample.cpu);
	}
	else if (entry->entry_type == HOTCAHCE_SAMPLE)
	{
		seq_printf(m, " HOTCACHE %4d %4d", entry->data.hotcache_sample.src_cpu, 
				entry->data.hotcache_sample.dst_cpu);
	}
	else if (entry->entry_type == IDLE_BALANCE_SAMPLE)
	{
		seq_printf(m, " IDLE_BALANCE %4d %4d", entry->data.idle_balance_sample.cpu,
				entry->data.idle_balance_sample.pulled_task);
	}
	else if (entry->entry_type == DOMAIN_FLAG_SAMPLE)
	{
		seq_printf(m, " FLAG %4d %4d %4d", entry->data.domain_flag_sample.cpu,
				entry->data.domain_flag_sample.level, entry->data.domain_flag_sample.flag);
	}
	else if (entry->entry_type == DOMIAN_INTERVAL_SAMPLE)
	{
		seq_printf(m, " INTERVAL %4d %4d %12u", entry->data.domain_interval_sample.cpu,
				entry->data.domain_interval_sample.level, entry->data.domain_interval_sample.interval);
	}
	else if (entry->entry_type == CGROUP_CPUMASK_SAMPLE)
	{
		seq_printf(m, " CPUMASK %4d", entry->data.cgroup_cpumask_sample.cpu);
	}
	else if (entry->entry_type == START_MIGRATION_SAMPLE)
	{
		seq_printf(m, " START_M %4d %4d", entry->data.start_migration_sample.src_cpu,
				entry->data.start_migration_sample.dst_cpu);
	}
	else if (entry->entry_type == END_MIGRATION_SAMPLE)
	{
		seq_printf(m, " END_M %4d %4d %4d", entry->data.end_migration_sample.src_cpu,
				entry->data.end_migration_sample.dst_cpu, entry->data.end_migration_sample.ld_moved);
	}
	else if (entry->entry_type == BUSIEST_SAMPLE)
	{
		seq_printf(m, " BS %4d %4d", entry->data.busiest_sample.flag, entry->data.busiest_sample.cpu);
	}
	else if (entry->entry_type == DETACH_STATUS_SAMPLE)
	{
		seq_printf(m, " DETACH_STAT %4d %12u %12u %12u %12ld", entry->data.detach_status_sample.cpu,
				entry->data.detach_status_sample.loop, entry->data.detach_status_sample.loop_max, 
				entry->data.detach_status_sample.loop_break, entry->data.detach_status_sample.imbalance);
	}
	else if (entry->entry_type == DETACH_LOAD_SAMPLE)
	{
		seq_printf(m, " DETACH_LOAD %12ld", entry->data.detach_load_sample.load);
	}

	seq_printf(m, "\n");
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
		set_sp_module_record_load_balance(NULL);
		set_sp_module_record_file_writing(NULL);
		set_sp_module_record_cpuallowed_change(NULL);
		set_sp_module_record_rebalance(NULL);
		set_sp_module_record_sd_cpus(NULL);
		set_sp_module_record_hotcache_rej(NULL);
		set_sp_module_record_idle_balance(NULL);
		set_sp_module_record_sd_flag(NULL);
		set_sp_module_record_sd_interval(NULL);
		set_sp_module_record_cgroup_cpumask(NULL);
		set_sp_module_record_start_migration(NULL);
		set_sp_module_record_end_migration(NULL);
		set_sp_module_record_busiest(NULL);
		set_sp_module_record_detach_status(NULL);
		set_sp_module_record_detach_load(NULL);
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

void sched_profiler_record_load_change(int flag, unsigned long load, int cpu)
{
	unsigned long __current_sample_entry_id;

	__current_sample_entry_id =
		__sync_fetch_and_add(&current_sample_entry_id, 1);

	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		set_sp_module_set_nr_running(NULL);
		set_sp_module_record_load_change(NULL);
		set_sp_module_record_load_balance(NULL);
		set_sp_module_record_file_writing(NULL);
		set_sp_module_record_cpuallowed_change(NULL);
		set_sp_module_record_rebalance(NULL);
		set_sp_module_record_sd_cpus(NULL);
		set_sp_module_record_hotcache_rej(NULL);
		set_sp_module_record_idle_balance(NULL);
		set_sp_module_record_sd_flag(NULL);
		set_sp_module_record_sd_interval(NULL);
		set_sp_module_record_cgroup_cpumask(NULL);
		set_sp_module_record_start_migration(NULL);
		set_sp_module_record_end_migration(NULL);
		set_sp_module_record_busiest(NULL);
		set_sp_module_record_detach_status(NULL);
		set_sp_module_record_detach_load(NULL);
		return;
	}

	sample_entries[__current_sample_entry_id]
		 .sched_clock = ktime_get_mono_fast_ns();
	sample_entries[__current_sample_entry_id]
		 .entry_type = LOAD_SAMPLE;
	sample_entries[__current_sample_entry_id].data.load_sample
		 .flag = flag;
	sample_entries[__current_sample_entry_id].data.load_sample
		 .load = load;
	sample_entries[__current_sample_entry_id].data.load_sample
		 .cpu = (unsigned char)cpu;
}

void sched_profiler_record_load_balance(int src_cpu, int dst_cpu, int ld_moved)
{
	unsigned long __current_sample_entry_id;

	__current_sample_entry_id =
		__sync_fetch_and_add(&current_sample_entry_id, 1);
	
	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		set_sp_module_set_nr_running(NULL);
		set_sp_module_record_load_change(NULL);
		set_sp_module_record_load_balance(NULL);
		set_sp_module_record_file_writing(NULL);
		set_sp_module_record_cpuallowed_change(NULL);
		set_sp_module_record_rebalance(NULL);
		set_sp_module_record_sd_cpus(NULL);
		set_sp_module_record_hotcache_rej(NULL);
		set_sp_module_record_idle_balance(NULL);
		set_sp_module_record_sd_flag(NULL);
		set_sp_module_record_sd_interval(NULL);
		set_sp_module_record_cgroup_cpumask(NULL);
		set_sp_module_record_start_migration(NULL);
		set_sp_module_record_end_migration(NULL);
		set_sp_module_record_busiest(NULL);
		set_sp_module_record_detach_status(NULL);
		set_sp_module_record_detach_load(NULL);
	}

	sample_entries[__current_sample_entry_id]
		 .entry_type = LOAD_BALANCE_SAMPLE;
	sample_entries[__current_sample_entry_id]
		 .sched_clock = ktime_get_mono_fast_ns();
	sample_entries[__current_sample_entry_id].data.load_balance_sample
		 .src_cpu = src_cpu;
	sample_entries[__current_sample_entry_id].data.load_balance_sample
		 .dst_cpu = dst_cpu;
	sample_entries[__current_sample_entry_id].data.load_balance_sample
		 .ld_moved = ld_moved;
	
}

void sched_profiler_record_file_writing(void)
{
	unsigned long __current_sample_entry_id;

	__current_sample_entry_id = 
		__sync_fetch_and_add(&current_sample_entry_id, 1);

	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		set_sp_module_set_nr_running(NULL);
		set_sp_module_record_load_change(NULL);
		set_sp_module_record_load_balance(NULL);
		set_sp_module_record_file_writing(NULL);
		set_sp_module_record_cpuallowed_change(NULL);
		set_sp_module_record_rebalance(NULL);
		set_sp_module_record_sd_cpus(NULL);
		set_sp_module_record_hotcache_rej(NULL);
		set_sp_module_record_idle_balance(NULL);
		set_sp_module_record_sd_flag(NULL);
		set_sp_module_record_sd_interval(NULL);
		set_sp_module_record_cgroup_cpumask(NULL);
		set_sp_module_record_start_migration(NULL);
		set_sp_module_record_end_migration(NULL);
		set_sp_module_record_busiest(NULL);
		set_sp_module_record_detach_status(NULL);
		set_sp_module_record_detach_load(NULL);
		return;
	}

	sample_entries[__current_sample_entry_id]
		 .sched_clock = ktime_get_mono_fast_ns();
	sample_entries[__current_sample_entry_id]
		 .entry_type = FILE_WRITING_SAMPLE;
}

void sched_profiler_record_cpuallowed_change(int cpu)
{
	unsigned long __current_sample_entry_id;

	__current_sample_entry_id = 
		__sync_fetch_and_add(&current_sample_entry_id, 1);

	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		set_sp_module_set_nr_running(NULL);
		set_sp_module_record_load_change(NULL);
		set_sp_module_record_load_balance(NULL);
		set_sp_module_record_file_writing(NULL);
		set_sp_module_record_cpuallowed_change(NULL);
		set_sp_module_record_rebalance(NULL);
		set_sp_module_record_sd_cpus(NULL);
		set_sp_module_record_hotcache_rej(NULL);
		set_sp_module_record_idle_balance(NULL);
		set_sp_module_record_sd_flag(NULL);
		set_sp_module_record_sd_interval(NULL);
		set_sp_module_record_cgroup_cpumask(NULL);
		set_sp_module_record_start_migration(NULL);
		set_sp_module_record_end_migration(NULL);
		set_sp_module_record_busiest(NULL);
		set_sp_module_record_detach_status(NULL);
		set_sp_module_record_detach_load(NULL);
		return;
	}

	sample_entries[__current_sample_entry_id]
		 .sched_clock = ktime_get_mono_fast_ns();
	sample_entries[__current_sample_entry_id]
		 .entry_type = CPUALLOWED_SAMPLE;
	sample_entries[__current_sample_entry_id].data.cpu_allowed_sample
		 .cpu = cpu;
}

void sched_profiler_record_rebalance(int cpu, int level, int r)
{
	unsigned long __current_sample_entry_id;

	__current_sample_entry_id = 
		__sync_fetch_and_add(&current_sample_entry_id, 1);

	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		set_sp_module_set_nr_running(NULL);
		set_sp_module_record_load_change(NULL);
		set_sp_module_record_load_balance(NULL);
		set_sp_module_record_file_writing(NULL);
		set_sp_module_record_cpuallowed_change(NULL);
		set_sp_module_record_rebalance(NULL);
		set_sp_module_record_sd_cpus(NULL);
		set_sp_module_record_hotcache_rej(NULL);
		set_sp_module_record_idle_balance(NULL);
		set_sp_module_record_sd_flag(NULL);
		set_sp_module_record_sd_interval(NULL);
		set_sp_module_record_cgroup_cpumask(NULL);
		set_sp_module_record_start_migration(NULL);
		set_sp_module_record_end_migration(NULL);
		set_sp_module_record_busiest(NULL);
		set_sp_module_record_detach_status(NULL);
		set_sp_module_record_detach_load(NULL);
		return;
	}

	sample_entries[__current_sample_entry_id]
		 .sched_clock = ktime_get_mono_fast_ns();
	sample_entries[__current_sample_entry_id]
		 .entry_type = REBALANCE_SAMPLE;
	sample_entries[__current_sample_entry_id].data.rebalance_sample
		 .cpu = cpu;
	sample_entries[__current_sample_entry_id].data.rebalance_sample
		 .level = level;
	sample_entries[__current_sample_entry_id].data.rebalance_sample
		 .r = r;

}

void sched_profiler_record_sd_cpus(int cpu)
{
	unsigned long __current_sample_entry_id;

	__current_sample_entry_id = 
		__sync_fetch_and_add(&current_sample_entry_id, 1);

	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		set_sp_module_set_nr_running(NULL);
		set_sp_module_record_load_change(NULL);
		set_sp_module_record_load_balance(NULL);
		set_sp_module_record_file_writing(NULL);
		set_sp_module_record_cpuallowed_change(NULL);
		set_sp_module_record_rebalance(NULL);
		set_sp_module_record_sd_cpus(NULL);
		set_sp_module_record_hotcache_rej(NULL);
		set_sp_module_record_idle_balance(NULL);
		set_sp_module_record_sd_flag(NULL);
		set_sp_module_record_sd_interval(NULL);
		set_sp_module_record_cgroup_cpumask(NULL);
		set_sp_module_record_start_migration(NULL);
		set_sp_module_record_end_migration(NULL);
		set_sp_module_record_busiest(NULL);
		set_sp_module_record_detach_status(NULL);
		set_sp_module_record_detach_load(NULL);
		return;
	}
	sample_entries[__current_sample_entry_id]
		 .sched_clock = ktime_get_mono_fast_ns();
	sample_entries[__current_sample_entry_id]
		 .entry_type = DOMAINCPU_SAMPLE;
	sample_entries[__current_sample_entry_id].data.domain_sample
		 .cpu = cpu;
}

void sched_profiler_record_hotcache_rej(int src_cpu, int dst_cpu)
{
	unsigned long __current_sample_entry_id;

	__current_sample_entry_id = 
		__sync_fetch_and_add(&current_sample_entry_id, 1);

	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		set_sp_module_set_nr_running(NULL);
		set_sp_module_record_load_change(NULL);
		set_sp_module_record_load_balance(NULL);
		set_sp_module_record_file_writing(NULL);
		set_sp_module_record_cpuallowed_change(NULL);
		set_sp_module_record_rebalance(NULL);
		set_sp_module_record_sd_cpus(NULL);
		set_sp_module_record_hotcache_rej(NULL);
		set_sp_module_record_idle_balance(NULL);
		set_sp_module_record_sd_flag(NULL);
		set_sp_module_record_sd_interval(NULL);
		set_sp_module_record_cgroup_cpumask(NULL);
		set_sp_module_record_start_migration(NULL);
		set_sp_module_record_end_migration(NULL);
		set_sp_module_record_busiest(NULL);
		set_sp_module_record_detach_status(NULL);
		set_sp_module_record_detach_load(NULL);
		return;
	}
	sample_entries[__current_sample_entry_id]
		 .sched_clock = ktime_get_mono_fast_ns();
	sample_entries[__current_sample_entry_id]
		 .entry_type = HOTCAHCE_SAMPLE;
	sample_entries[__current_sample_entry_id].data.hotcache_sample
		 .src_cpu = src_cpu;
	sample_entries[__current_sample_entry_id].data.hotcache_sample
		 .dst_cpu = dst_cpu;
		
}

void sched_profiler_record_idle_balance(int cpu, int pulled_task)
{
	unsigned long __current_sample_entry_id;

	__current_sample_entry_id = 
		__sync_fetch_and_add(&current_sample_entry_id, 1);

	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		set_sp_module_set_nr_running(NULL);
		set_sp_module_record_load_change(NULL);
		set_sp_module_record_load_balance(NULL);
		set_sp_module_record_file_writing(NULL);
		set_sp_module_record_cpuallowed_change(NULL);
		set_sp_module_record_rebalance(NULL);
		set_sp_module_record_sd_cpus(NULL);
		set_sp_module_record_hotcache_rej(NULL);
		set_sp_module_record_idle_balance(NULL);
		set_sp_module_record_sd_flag(NULL);
		set_sp_module_record_sd_interval(NULL);
		set_sp_module_record_cgroup_cpumask(NULL);
		set_sp_module_record_start_migration(NULL);
		set_sp_module_record_end_migration(NULL);
		set_sp_module_record_busiest(NULL);
		set_sp_module_record_detach_status(NULL);
		set_sp_module_record_detach_load(NULL);
		return;
	}
	sample_entries[__current_sample_entry_id]
		 .sched_clock = ktime_get_mono_fast_ns();
	sample_entries[__current_sample_entry_id]
		 .entry_type = IDLE_BALANCE_SAMPLE;
	sample_entries[__current_sample_entry_id].data.idle_balance_sample
		 .cpu = cpu;
	sample_entries[__current_sample_entry_id].data.idle_balance_sample
		 .pulled_task = pulled_task;
}

void sched_profiler_record_sd_flag(int cpu, int level, int flag)
{
	unsigned long __current_sample_entry_id;

	__current_sample_entry_id = 
		__sync_fetch_and_add(&current_sample_entry_id, 1);

	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		set_sp_module_set_nr_running(NULL);
		set_sp_module_record_load_change(NULL);
		set_sp_module_record_load_balance(NULL);
		set_sp_module_record_file_writing(NULL);
		set_sp_module_record_cpuallowed_change(NULL);
		set_sp_module_record_rebalance(NULL);
		set_sp_module_record_sd_cpus(NULL);
		set_sp_module_record_hotcache_rej(NULL);
		set_sp_module_record_idle_balance(NULL);
		set_sp_module_record_sd_flag(NULL);
		set_sp_module_record_sd_interval(NULL);
		set_sp_module_record_cgroup_cpumask(NULL);
		set_sp_module_record_start_migration(NULL);
		set_sp_module_record_end_migration(NULL);
		set_sp_module_record_busiest(NULL);
		set_sp_module_record_detach_status(NULL);
		set_sp_module_record_detach_load(NULL);
		return;
	}
	sample_entries[__current_sample_entry_id]
		 .sched_clock = ktime_get_mono_fast_ns();
	sample_entries[__current_sample_entry_id]
		 .entry_type = DOMAIN_FLAG_SAMPLE;
	sample_entries[__current_sample_entry_id].data.domain_flag_sample
		 .cpu = cpu;
	sample_entries[__current_sample_entry_id].data.domain_flag_sample
		 .level = level;
	sample_entries[__current_sample_entry_id].data.domain_flag_sample
		 .flag = flag;
}

void sched_profiler_record_sd_interval(int cpu, int level, unsigned int interval)
{
	unsigned long __current_sample_entry_id;

	__current_sample_entry_id = 
		__sync_fetch_and_add(&current_sample_entry_id, 1);

	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		set_sp_module_set_nr_running(NULL);
		set_sp_module_record_load_change(NULL);
		set_sp_module_record_load_balance(NULL);
		set_sp_module_record_file_writing(NULL);
		set_sp_module_record_cpuallowed_change(NULL);
		set_sp_module_record_rebalance(NULL);
		set_sp_module_record_sd_cpus(NULL);
		set_sp_module_record_hotcache_rej(NULL);
		set_sp_module_record_idle_balance(NULL);
		set_sp_module_record_sd_flag(NULL);
		set_sp_module_record_sd_interval(NULL);
		set_sp_module_record_cgroup_cpumask(NULL);
		set_sp_module_record_start_migration(NULL);
		set_sp_module_record_end_migration(NULL);
		set_sp_module_record_busiest(NULL);
		set_sp_module_record_detach_status(NULL);
		set_sp_module_record_detach_load(NULL);
		return;
	}
	sample_entries[__current_sample_entry_id]
		 .sched_clock = ktime_get_mono_fast_ns();
	sample_entries[__current_sample_entry_id]
		 .entry_type = DOMIAN_INTERVAL_SAMPLE;
	sample_entries[__current_sample_entry_id].data.domain_interval_sample
		 .cpu = cpu;
	sample_entries[__current_sample_entry_id].data.domain_interval_sample
		 .level = level;
	sample_entries[__current_sample_entry_id].data.domain_interval_sample
		 .interval = interval;
}

void sched_profiler_record_cgroup_cpumask(int cpu)
{
	unsigned long __current_sample_entry_id;

	__current_sample_entry_id = 
		__sync_fetch_and_add(&current_sample_entry_id, 1);

	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		set_sp_module_set_nr_running(NULL);
		set_sp_module_record_load_change(NULL);
		set_sp_module_record_load_balance(NULL);
		set_sp_module_record_file_writing(NULL);
		set_sp_module_record_cpuallowed_change(NULL);
		set_sp_module_record_rebalance(NULL);
		set_sp_module_record_sd_cpus(NULL);
		set_sp_module_record_hotcache_rej(NULL);
		set_sp_module_record_idle_balance(NULL);
		set_sp_module_record_sd_flag(NULL);
		set_sp_module_record_sd_interval(NULL);
		set_sp_module_record_cgroup_cpumask(NULL);
		set_sp_module_record_start_migration(NULL);
		set_sp_module_record_end_migration(NULL);
		set_sp_module_record_busiest(NULL);
		set_sp_module_record_detach_status(NULL);
		set_sp_module_record_detach_load(NULL);
		return;
	}
	sample_entries[__current_sample_entry_id]
		 .sched_clock = ktime_get_mono_fast_ns();
	sample_entries[__current_sample_entry_id]
		 .entry_type = CGROUP_CPUMASK_SAMPLE;
	sample_entries[__current_sample_entry_id].data.cgroup_cpumask_sample
		 .cpu = cpu;
}

void sched_profiler_record_start_migration(int src_cpu, int dst_cpu)
{
	unsigned long __current_sample_entry_id;

	__current_sample_entry_id = 
		__sync_fetch_and_add(&current_sample_entry_id, 1);

	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		set_sp_module_set_nr_running(NULL);
		set_sp_module_record_load_change(NULL);
		set_sp_module_record_load_balance(NULL);
		set_sp_module_record_file_writing(NULL);
		set_sp_module_record_cpuallowed_change(NULL);
		set_sp_module_record_rebalance(NULL);
		set_sp_module_record_sd_cpus(NULL);
		set_sp_module_record_hotcache_rej(NULL);
		set_sp_module_record_idle_balance(NULL);
		set_sp_module_record_sd_flag(NULL);
		set_sp_module_record_sd_interval(NULL);
		set_sp_module_record_cgroup_cpumask(NULL);
		set_sp_module_record_start_migration(NULL);
		set_sp_module_record_end_migration(NULL);
		set_sp_module_record_busiest(NULL);
		set_sp_module_record_detach_status(NULL);
		return;
	}
	sample_entries[__current_sample_entry_id]
		 .sched_clock = ktime_get_mono_fast_ns();
	sample_entries[__current_sample_entry_id]
		 .entry_type = START_MIGRATION_SAMPLE;
	sample_entries[__current_sample_entry_id].data.start_migration_sample
		 .src_cpu = src_cpu;
	sample_entries[__current_sample_entry_id].data.start_migration_sample
		 .dst_cpu = dst_cpu;
}

void sched_profiler_record_end_migration(int src_cpu, int dst_cpu, int ld_moved)
{
	unsigned long __current_sample_entry_id;

	__current_sample_entry_id = 
		__sync_fetch_and_add(&current_sample_entry_id, 1);

	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		set_sp_module_set_nr_running(NULL);
		set_sp_module_record_load_change(NULL);
		set_sp_module_record_load_balance(NULL);
		set_sp_module_record_file_writing(NULL);
		set_sp_module_record_cpuallowed_change(NULL);
		set_sp_module_record_rebalance(NULL);
		set_sp_module_record_sd_cpus(NULL);
		set_sp_module_record_hotcache_rej(NULL);
		set_sp_module_record_idle_balance(NULL);
		set_sp_module_record_sd_flag(NULL);
		set_sp_module_record_sd_interval(NULL);
		set_sp_module_record_cgroup_cpumask(NULL);
		set_sp_module_record_start_migration(NULL);
		set_sp_module_record_end_migration(NULL);
		set_sp_module_record_busiest(NULL);
		set_sp_module_record_detach_status(NULL);
		set_sp_module_record_detach_load(NULL);
		return;
	}
	sample_entries[__current_sample_entry_id]
		 .sched_clock = ktime_get_mono_fast_ns();
	sample_entries[__current_sample_entry_id]
		 .entry_type = END_MIGRATION_SAMPLE;
	sample_entries[__current_sample_entry_id].data.end_migration_sample
		 .src_cpu = src_cpu;
	sample_entries[__current_sample_entry_id].data.end_migration_sample
		 .dst_cpu = dst_cpu;
	sample_entries[__current_sample_entry_id].data.end_migration_sample
		 .ld_moved = ld_moved;
}

void sched_profiler_record_busiest(int flag, int cpu)
{
	unsigned long __current_sample_entry_id;

	__current_sample_entry_id = 
		__sync_fetch_and_add(&current_sample_entry_id, 1);

	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		set_sp_module_set_nr_running(NULL);
		set_sp_module_record_load_change(NULL);
		set_sp_module_record_load_balance(NULL);
		set_sp_module_record_file_writing(NULL);
		set_sp_module_record_cpuallowed_change(NULL);
		set_sp_module_record_rebalance(NULL);
		set_sp_module_record_sd_cpus(NULL);
		set_sp_module_record_hotcache_rej(NULL);
		set_sp_module_record_idle_balance(NULL);
		set_sp_module_record_sd_flag(NULL);
		set_sp_module_record_sd_interval(NULL);
		set_sp_module_record_cgroup_cpumask(NULL);
		set_sp_module_record_start_migration(NULL);
		set_sp_module_record_end_migration(NULL);
		set_sp_module_record_busiest(NULL);
		set_sp_module_record_detach_load(NULL);
		return;
	}
	sample_entries[__current_sample_entry_id]
		 .sched_clock = ktime_get_mono_fast_ns();
	sample_entries[__current_sample_entry_id]
		 .entry_type = BUSIEST_SAMPLE;
	sample_entries[__current_sample_entry_id].data.busiest_sample
		 .flag = flag;
	sample_entries[__current_sample_entry_id].data.busiest_sample
		 .cpu = cpu;
}

void sched_profiler_record_detach_status(int src_cpu, unsigned int loop, unsigned int loop_max, unsigned int loop_break, long imbalance)
{
	unsigned long __current_sample_entry_id;

	__current_sample_entry_id = 
		__sync_fetch_and_add(&current_sample_entry_id, 1);

	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		set_sp_module_set_nr_running(NULL);
		set_sp_module_record_load_change(NULL);
		set_sp_module_record_load_balance(NULL);
		set_sp_module_record_file_writing(NULL);
		set_sp_module_record_cpuallowed_change(NULL);
		set_sp_module_record_rebalance(NULL);
		set_sp_module_record_sd_cpus(NULL);
		set_sp_module_record_hotcache_rej(NULL);
		set_sp_module_record_idle_balance(NULL);
		set_sp_module_record_sd_flag(NULL);
		set_sp_module_record_sd_interval(NULL);
		set_sp_module_record_cgroup_cpumask(NULL);
		set_sp_module_record_start_migration(NULL);
		set_sp_module_record_end_migration(NULL);
		set_sp_module_record_busiest(NULL);
		set_sp_module_record_detach_load(NULL);
		return;
	}
	sample_entries[__current_sample_entry_id]
		 .sched_clock = ktime_get_mono_fast_ns();
	sample_entries[__current_sample_entry_id]
		 .entry_type = DETACH_STATUS_SAMPLE;
	sample_entries[__current_sample_entry_id].data.detach_status_sample
		 .cpu = src_cpu;
	sample_entries[__current_sample_entry_id].data.detach_status_sample
		 .loop = loop;
	sample_entries[__current_sample_entry_id].data.detach_status_sample
		 .loop_max = loop_max;
	sample_entries[__current_sample_entry_id].data.detach_status_sample
		 .loop_break = loop_break;
	sample_entries[__current_sample_entry_id].data.detach_status_sample
		 .imbalance = imbalance;
}

void sched_profiler_record_detach_load(unsigned long load)
{
	unsigned long __current_sample_entry_id;

	__current_sample_entry_id = 
		__sync_fetch_and_add(&current_sample_entry_id, 1);

	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		set_sp_module_set_nr_running(NULL);
		set_sp_module_record_load_change(NULL);
		set_sp_module_record_load_balance(NULL);
		set_sp_module_record_file_writing(NULL);
		set_sp_module_record_cpuallowed_change(NULL);
		set_sp_module_record_rebalance(NULL);
		set_sp_module_record_sd_cpus(NULL);
		set_sp_module_record_hotcache_rej(NULL);
		set_sp_module_record_idle_balance(NULL);
		set_sp_module_record_sd_flag(NULL);
		set_sp_module_record_sd_interval(NULL);
		set_sp_module_record_cgroup_cpumask(NULL);
		set_sp_module_record_start_migration(NULL);
		set_sp_module_record_end_migration(NULL);
		set_sp_module_record_busiest(NULL);
		set_sp_module_record_detach_load(NULL);
		return;
	}
	sample_entries[__current_sample_entry_id]
		 .sched_clock = ktime_get_mono_fast_ns();
	sample_entries[__current_sample_entry_id]
		 .entry_type = DETACH_LOAD_SAMPLE;
	sample_entries[__current_sample_entry_id].data.detach_load_sample
		 .load = load;
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
	set_sp_module_record_load_change(sched_profiler_record_load_change);
	set_sp_module_record_load_balance(sched_profiler_record_load_balance);
	set_sp_module_record_file_writing(sched_profiler_record_file_writing);
	set_sp_module_record_cpuallowed_change(sched_profiler_record_cpuallowed_change);
	set_sp_module_record_rebalance(sched_profiler_record_rebalance);
	set_sp_module_record_sd_cpus(sched_profiler_record_sd_cpus);
	set_sp_module_record_hotcache_rej(sched_profiler_record_hotcache_rej);
	set_sp_module_record_idle_balance(sched_profiler_record_idle_balance);
	set_sp_module_record_sd_flag(sched_profiler_record_sd_flag);
	set_sp_module_record_sd_interval(sched_profiler_record_sd_interval);
	set_sp_module_record_cgroup_cpumask(sched_profiler_record_cgroup_cpumask);
	set_sp_module_record_start_migration(sched_profiler_record_start_migration);
	set_sp_module_record_end_migration(sched_profiler_record_end_migration);
	set_sp_module_record_busiest(sched_profiler_record_busiest);
	set_sp_module_record_detach_status(sched_profiler_record_detach_status);
	set_sp_module_record_detach_load(sched_profiler_record_detach_load);

	return 0;
}

void cleanup_module(void)
{
	set_sp_module_set_nr_running(NULL);
	set_sp_module_record_load_change(NULL);
	set_sp_module_record_load_balance(NULL);
	set_sp_module_record_file_writing(NULL);
	set_sp_module_record_cpuallowed_change(NULL);
	set_sp_module_record_rebalance(NULL);
	set_sp_module_record_sd_cpus(NULL);
	set_sp_module_record_hotcache_rej(NULL);
	set_sp_module_record_idle_balance(NULL);
	set_sp_module_record_sd_flag(NULL);
	set_sp_module_record_sd_interval(NULL);
	set_sp_module_record_cgroup_cpumask(NULL);
	set_sp_module_record_start_migration(NULL);
	set_sp_module_record_end_migration(NULL);
	set_sp_module_record_busiest(NULL);
	set_sp_module_record_detach_status(NULL);
	set_sp_module_record_detach_load(NULL);

	ssleep(1);

	vfree(sample_entries);
	remove_proc_entry("sched_profiler", NULL);
}

MODULE_LICENSE("GPL");

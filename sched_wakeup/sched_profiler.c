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
	FORBID_TASK_CPU_SAMPLE,
	SELECT_TASK_SAMPLE,
	TOP_SD_SAMPLE,
	EACH_SD_SAMPLE,
	IDLEST_GROUP_SAMPLE,
	RQ_SIZE_SAMPLE,
	CPU_SELECTED_SAMPLE
};

struct sched_group {
	struct sched_group *next;	/* Must be a circular list */
	atomic_t ref;

	unsigned int group_weight;
	struct sched_group_capacity *sgc;
	int asym_prefer_cpu;		/* cpu of highest priority in group */
	unsigned long cpumask[0];
};

struct sched_domain {
	/* These fields must be setup */
	struct sched_domain *parent;	/* top domain must be null terminated */
	struct sched_domain *child;	/* bottom domain must be null terminated */
	struct sched_group *groups;	/* the balancing groups of the domain */
	unsigned long min_interval;	/* Minimum balance interval ms */
	unsigned long max_interval;	/* Maximum balance interval ms */
	unsigned int busy_factor;	/* less balancing by factor if busy */
	unsigned int imbalance_pct;	/* No balance until over watermark */
	unsigned int cache_nice_tries;	/* Leave cache hot tasks for # tries */
	unsigned int busy_idx;
	unsigned int idle_idx;
	unsigned int newidle_idx;
	unsigned int wake_idx;
	unsigned int forkexec_idx;
	unsigned int smt_gain;

	int nohz_idle;			/* NOHZ IDLE status */
	int flags;			/* See SD_* */
	int level;

	/* Runtime fields. */
	unsigned long last_balance;	/* init to jiffies. units in jiffies */
	unsigned int balance_interval;	/* initialise to 1. units in ms. */
	unsigned int nr_balance_failed; /* initialise to 0 */

	/* idle_balance() stats */
	u64 max_newidle_lb_cost;
	unsigned long next_decay_max_lb_cost;

	u64 avg_scan_cost;		/* select_idle_sibling */

#ifdef CONFIG_SCHED_DEBUG
	char *name;
#endif
	union {
		void *private;		/* used during construction */
		struct rcu_head rcu;	/* used during destruction */
	};
	struct sched_domain_shared *shared;

	unsigned int span_weight;
	/*
	 * Span of all CPUs in this domain.
	 *
	 * NOTE: this field is variable length. (Allocated dynamically
	 * by attaching extra space to the end of the structure,
	 * depending on how many CPUs the kernel has booted up with)
	 */
	unsigned long span[0];
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
            int cpus_in_mask[41];
        } cpus_allowed_sample;
		struct {
			int ret;
			int newmask[40];
		} forbid_task_cpu_sample;
		struct {
			int allowed_cpus[41];
			int cpu;
			int pid;
		} select_task_sample;
		struct {
			int top_sd_cpus[41];
		} top_sd_sample;
		struct {
			int each_sd_cpus[41];
			unsigned int imbalance_pct;
		} each_sd_sample;
		struct {
			int idlest_group_cpus[41];
			unsigned long min_load; 
			unsigned long this_load;
		} idlest_group_sample;
		struct {
			unsigned rq_size;
			int cpu;
		} rq_size_sample;
		struct {
			int selected_cpu;
			int stage;
		} cpu_selected_sample;
		
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
typedef void (*forbid_task_cpu_t)(struct task_struct *, int);
typedef void (*record_select_task_func_t)(struct task_struct *, int);
typedef void (*record_top_sd_t)(struct sched_domain *);
typedef void (*record_each_sd_t)(struct sched_domain *);
typedef void (*record_idlest_group_t)(struct sched_group *, unsigned long, unsigned long);
typedef void (*record_nr_running_t)(unsigned, int);
typedef void (*record_cpu_selected_t)(int, int);


/******************************************************************************/
/* Prototypes								      */
/******************************************************************************/
extern void set_sp_module_record_wakeup_paras
	(record_wakeup_paras_t __sp_module_record_wakeup_paras);
extern void set_sp_module_record_cpus_allowed
	(record_cpus_allowed_t __sp_module_record_cpus_allowed);
extern void set_sp_module_forbid_task_cpu
    (forbid_task_cpu_t __sp_module_forbit_task_cpu);
extern void set_sp_module_record_select_task_func
	(record_select_task_func_t __sp_module_record_select_task_func);
extern void set_sp_module_record_top_sd
	(record_top_sd_t __sp_module_record_top_sd);
extern void set_sp_module_record_each_sd
	(record_each_sd_t __sp_module_record_each_sd);
extern void set_sp_module_record_idlest_group
	(record_idlest_group_t __sp_module_record_idlest_group);
extern void set_sp_module_record_nr_running
	(record_nr_running_t __sp_module_record_nr_running);
extern void set_sp_module_record_cpu_selected
	(record_cpu_selected_t __sp_module_record_cpu_selected);

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
	set_sp_module_record_wakeup_paras(NULL);
	set_sp_module_record_cpus_allowed(NULL);

	n_sample_entries = current_sample_entry_id < MAX_SAMPLE_ENTRIES ?
			   current_sample_entry_id : MAX_SAMPLE_ENTRIES;

	entry = &sample_entries[iteration];
	seq_printf(m, "	 %llu nsecs ", entry->sched_clock);

	if (entry->entry_type == RQ_SIZE_SAMPLE)
	{
		rq_size[entry->data.rq_size_sample.cpu] =
			entry->data.rq_size_sample.rq_size;
	}

	if (entry->entry_type == WAKEUP_PARAS_SAMPLE)
	{
        seq_printf(m, " WAKEUP        %4d %4d %12d", 
                        entry->data.wakeup_paras_sample.cur_cpu,
                        entry->data.wakeup_paras_sample.on_rq,
                        entry->data.wakeup_paras_sample.parent_pid);
	}
	if (entry->entry_type == CPUS_ALLOWED_SAMPLE)
	{	seq_printf(m, " ALLOWED_CPUS  ");
		for(i = 0; i < 64; i++) {
            if (entry->data.cpus_allowed_sample.cpus_in_mask[i] == -1) break;
            seq_printf(m, "%4d, ", entry->data.cpus_allowed_sample.cpus_in_mask[i]);
        }
	}
	if (entry->entry_type == FORBID_TASK_CPU_SAMPLE)
	{
		seq_printf(m, " FORBID_CPU_RET %4d ", 
		           entry->data.forbid_task_cpu_sample.ret);
		seq_printf(m, " NEW_MASK       ");
		for(i = 0; i < 40; i++) {
            if (entry->data.forbid_task_cpu_sample.newmask[i] == -1) break;
            seq_printf(m, "%d, ", entry->data.forbid_task_cpu_sample.newmask[i]);
        }
	}
	if (entry->entry_type == SELECT_TASK_SAMPLE)
	{
		seq_printf(m, "SELECT_TASK_CPU %4d %4d ", 
				   entry->data.select_task_sample.cpu,
				   entry->data.select_task_sample.pid);
		seq_printf(m, " ALLOWED_CPUS  ");
		for(i = 0; i < 64; i++) {
            if (entry->data.select_task_sample.allowed_cpus[i] == -1) break;
            seq_printf(m, "%4d, ", entry->data.select_task_sample.allowed_cpus[i]);
        }

	}
	if (entry->entry_type == TOP_SD_SAMPLE)
	{
		seq_printf(m, " TOP_SD_CPUS   ");
		for(i = 0; i < 40; i++) {
            if (entry->data.top_sd_sample.top_sd_cpus[i] == -1) break;
            seq_printf(m, "%4d, ", entry->data.top_sd_sample.top_sd_cpus[i]);
        }
	}
	if (entry->entry_type == EACH_SD_SAMPLE)
	{
		seq_printf(m, " EACH_SD_CPUS  ");	
		for(i = 0; i < 64; i++) {
            if (entry->data.each_sd_sample.each_sd_cpus[i] == -1) break;
            seq_printf(m, "%4d, ", entry->data.each_sd_sample.each_sd_cpus[i]);
        }
		seq_printf(m, "%u ", entry->data.each_sd_sample.imbalance_pct);
	}
	if (entry->entry_type == IDLEST_GROUP_SAMPLE)
	{
		seq_printf(m, " IDLEST_GROUP  ");	
		for(i = 0; i < 64; i++) {
            if (entry->data.idlest_group_sample.idlest_group_cpus[i] == -1) break;
            seq_printf(m, "%4d, ", entry->data.idlest_group_sample.idlest_group_cpus[i]);
        }
		seq_printf(m, " MIN_LOAD  ");	
		seq_printf(m, "%lu ", entry->data.idlest_group_sample.min_load);
		seq_printf(m, " THIS_LOAD ");	
		seq_printf(m, "%lu, ", entry->data.idlest_group_sample.this_load);
	}
	if (entry->entry_type == RQ_SIZE_SAMPLE)
	{
		for (i = 0; i < MAX_CPUS; i++)
			seq_printf(m, " RQ %4d", rq_size[i]);
		seq_printf(m, " RQ %4d %4d", entry->data.rq_size_sample.cpu,
			   entry->data.rq_size_sample.rq_size);
	}
	if (entry->entry_type == CPU_SELECTED_SAMPLE)
	{
		seq_printf(m, " SELECTED_CPU  %4d %4d", 
					entry->data.cpu_selected_sample.selected_cpu,
					entry->data.cpu_selected_sample.stage );
	}
	seq_printf(m, "\n");
}



void clear_all_modules(void) {
	set_sp_module_record_wakeup_paras(NULL);
	set_sp_module_record_cpus_allowed(NULL);
	set_sp_module_forbid_task_cpu(NULL);
	set_sp_module_record_select_task_func(NULL);
	set_sp_module_record_top_sd(NULL);
	set_sp_module_record_each_sd(NULL);
	set_sp_module_record_idlest_group(NULL);
	set_sp_module_record_nr_running(NULL);
	set_sp_module_record_cpu_selected(NULL);
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
		clear_all_modules();
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
	int i,j = 0;

	__current_sample_entry_id =
		__sync_fetch_and_add(&current_sample_entry_id, 1);

	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		clear_all_modules();
		return;
	}

	sample_entries[__current_sample_entry_id]
		 .sched_clock = ktime_get_mono_fast_ns();
	sample_entries[__current_sample_entry_id]
		 .entry_type = CPUS_ALLOWED_SAMPLE;
	for_each_cpu(i, m) {
		sample_entries[__current_sample_entry_id]
		 .data.cpus_allowed_sample.cpus_in_mask[j++] = i;
	}
	sample_entries[__current_sample_entry_id]
		 .data.cpus_allowed_sample.cpus_in_mask[j++] = -1;
}

struct custom_cpuset {
	cpumask_var_t cpus_allowed;
};
void sched_profiler_forbid_task_cpu(struct task_struct * p, int forbid_cpu) {
	struct custom_cpuset _custom_cpuset = {
	};
	unsigned long __current_sample_entry_id;
	int i, j = 0, k;

	__current_sample_entry_id =
		__sync_fetch_and_add(&current_sample_entry_id, 1);

	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		clear_all_modules();
		return;
	}

	alloc_cpumask_var(&_custom_cpuset.cpus_allowed, GFP_KERNEL);
	cpumask_clear(_custom_cpuset.cpus_allowed);
	if((task_cpu(p)>=10 && task_cpu(p) <=19) || (task_cpu(p)>=30&&task_cpu(p)<=39)) {
		for (k = 0; k < 40; k = k+1) 
			if((k>=0 && k <=9) || (k>=20&&k<=29))
				cpumask_set_cpu(k, _custom_cpuset.cpus_allowed);
	}
	else {
		for (k = 0; k < 40; k = k+1)
			if((k>=10 && k <=19) || (k>=30&&k<=39))
				cpumask_set_cpu(k, _custom_cpuset.cpus_allowed);
	}
	if (p->real_parent->pid == 4875)
		set_cpus_allowed_ptr(p, _custom_cpuset.cpus_allowed);

	sample_entries[__current_sample_entry_id]
		 .sched_clock = ktime_get_mono_fast_ns();
	for_each_cpu(i, _custom_cpuset.cpus_allowed) {
		sample_entries[__current_sample_entry_id]
		 .data.forbid_task_cpu_sample.newmask[j++] = i;
	}
	sample_entries[__current_sample_entry_id]
		 .data.forbid_task_cpu_sample.newmask[j++] = -1;
	sample_entries[__current_sample_entry_id]
		 .entry_type = FORBID_TASK_CPU_SAMPLE;
	sample_entries[__current_sample_entry_id]
		 .data.forbid_task_cpu_sample.ret = task_cpu(p);
}

void sched_profiler_record_select_task(struct task_struct *p, int cpu) {
	unsigned long __current_sample_entry_id;
	int i,j = 0;

	__current_sample_entry_id =
		__sync_fetch_and_add(&current_sample_entry_id, 1);

	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		clear_all_modules();
		return;
	}

	sample_entries[__current_sample_entry_id]
		 .sched_clock = ktime_get_mono_fast_ns();
	sample_entries[__current_sample_entry_id]
		 .entry_type = SELECT_TASK_SAMPLE;
	sample_entries[__current_sample_entry_id]
		 .data.select_task_sample.pid = p->pid;
	for_each_cpu(i, &p->cpus_allowed) {
		sample_entries[__current_sample_entry_id]
		 .data.select_task_sample.allowed_cpus[j++] = i;
	}
	sample_entries[__current_sample_entry_id]
		 .data.select_task_sample.allowed_cpus[j++] = -1;
	sample_entries[__current_sample_entry_id]
		 .data.select_task_sample.cpu = cpu;

}

void sched_profiler_record_top_sd(struct sched_domain * sd) {
	unsigned long __current_sample_entry_id;
	int i,j = 0;

	__current_sample_entry_id =
		__sync_fetch_and_add(&current_sample_entry_id, 1);

	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		clear_all_modules();
		return;
	}

	sample_entries[__current_sample_entry_id]
		 .sched_clock = ktime_get_mono_fast_ns();
	sample_entries[__current_sample_entry_id]
		 .entry_type = TOP_SD_SAMPLE;
	if (sd != NULL) {
		for_each_cpu(i, to_cpumask(sd->span)) {
			sample_entries[__current_sample_entry_id]
				.data.top_sd_sample.top_sd_cpus[j++] = i;
		}
	}
	sample_entries[__current_sample_entry_id]
		.data.top_sd_sample.top_sd_cpus[j++] = -1;
}

void sched_profiler_record_each_sd(struct sched_domain * sd) {
	unsigned long __current_sample_entry_id;
	int i,j = 0;

	__current_sample_entry_id =
		__sync_fetch_and_add(&current_sample_entry_id, 1);

	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		clear_all_modules();
		return;
	}

	sample_entries[__current_sample_entry_id]
		 .sched_clock = ktime_get_mono_fast_ns();
	sample_entries[__current_sample_entry_id]
		 .entry_type = EACH_SD_SAMPLE;
	if (sd != NULL) {
		for_each_cpu(i, to_cpumask(sd->span)) {
			sample_entries[__current_sample_entry_id]
			.data.each_sd_sample.each_sd_cpus[j++] = i;
		}
	}
	sample_entries[__current_sample_entry_id]
		 .data.each_sd_sample.each_sd_cpus[j++] = -1;
	sample_entries[__current_sample_entry_id]
		 .data.each_sd_sample.imbalance_pct = sd->imbalance_pct;
}

void sched_profiler_record_idlest_group(struct sched_group * sg, unsigned long min_load, unsigned long this_load) {
	unsigned long __current_sample_entry_id;
	int i,j = 0;

	__current_sample_entry_id =
		__sync_fetch_and_add(&current_sample_entry_id, 1);

	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		clear_all_modules();
		return;
	}

	sample_entries[__current_sample_entry_id]
		 .sched_clock = ktime_get_mono_fast_ns();
	sample_entries[__current_sample_entry_id]
		 .entry_type = IDLEST_GROUP_SAMPLE;
	sample_entries[__current_sample_entry_id]
		 .data.idlest_group_sample.min_load = min_load;
	if (this_load + 125 < min_load)
		sample_entries[__current_sample_entry_id]
			.data.idlest_group_sample.this_load = 1;
	else
		sample_entries[__current_sample_entry_id]
			.data.idlest_group_sample.this_load = 0;
	if (sg != NULL) {
		for_each_cpu(i, to_cpumask(sg->cpumask)) {
			sample_entries[__current_sample_entry_id]
			.data.idlest_group_sample.idlest_group_cpus[j++] = i;
		}
	}
	sample_entries[__current_sample_entry_id]
		 .data.idlest_group_sample.idlest_group_cpus[j++] = -1;
}

void sched_profiler_record_nr_running(unsigned nr, int cpu) {
	unsigned long __current_sample_entry_id;

	__current_sample_entry_id =
		__sync_fetch_and_add(&current_sample_entry_id, 1);

	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		clear_all_modules();
		return;
	}

	sample_entries[__current_sample_entry_id]
		 .sched_clock = ktime_get_mono_fast_ns();
	sample_entries[__current_sample_entry_id]
		 .entry_type = RQ_SIZE_SAMPLE;
	sample_entries[__current_sample_entry_id]
		 .data.rq_size_sample.rq_size = nr;
	sample_entries[__current_sample_entry_id]
		 .data.rq_size_sample.cpu = cpu;

}

void sched_profiler_record_cpu_selected(int cpu, int stage) {
	unsigned long __current_sample_entry_id;

	__current_sample_entry_id =
		__sync_fetch_and_add(&current_sample_entry_id, 1);

	if (__current_sample_entry_id >= MAX_SAMPLE_ENTRIES)
	{
		clear_all_modules();
		return;
	}

	sample_entries[__current_sample_entry_id]
		 .sched_clock = ktime_get_mono_fast_ns();
	sample_entries[__current_sample_entry_id]
		 .entry_type = SELECT_TASK_SAMPLE;
	sample_entries[__current_sample_entry_id]
		 .data.cpu_selected_sample.selected_cpu = cpu;
	sample_entries[__current_sample_entry_id]
		 .data.cpu_selected_sample.stage = stage;
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
	set_sp_module_forbid_task_cpu(sched_profiler_forbid_task_cpu);
	set_sp_module_record_select_task_func(sched_profiler_record_select_task);
	set_sp_module_record_top_sd(sched_profiler_record_top_sd);
	set_sp_module_record_each_sd(sched_profiler_record_each_sd);
	set_sp_module_record_idlest_group(sched_profiler_record_idlest_group);
	set_sp_module_record_nr_running(sched_profiler_record_nr_running);
	set_sp_module_record_cpu_selected(sched_profiler_record_cpu_selected);

	return 0;
}

void cleanup_module(void)
{
	clear_all_modules();

	ssleep(1);

	vfree(sample_entries);
	remove_proc_entry("sched_profiler", NULL);
}

MODULE_LICENSE("GPL");

#include <linux/linkage.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>

#include <asm/tlbflush.h>

#include <asm/pgtable.h>
//#include <linux/mm.h>

//cgmin pmc
/*
struct pmc_data
{
		unsigned long addr;
		unsigned int loaded_mm_asid;
};
*/
long dwc(unsigned long src, unsigned long dst, unsigned long size) // double walk and change
{
	struct pmc_data data;
	data.loaded_mm_asid = this_cpu_read(cpu_tlbstate.loaded_mm_asid);

	struct	mm_struct *mm;

	pgd_t *pgd_src,*pgd_dst;
	p4d_t *p4d_src,*p4d_dst;
	pud_t *pud_src,*pud_dst;
	pmd_t *pmd_src,*pmd_dst;
	pte_t *pte_src,*pte_dst;

	unsigned long address_src,address_dst;
	long rv;

	unsigned long cnt;
	unsigned long pte_dst_end,pmd_dst_end,p4d_dst_end,pud_dst_end;
	unsigned long pte_src_end,pmd_src_end,p4d_src_end,pud_src_end;
	pte_t pte_temp;

	spinlock_t *ptl_src,*ptl_dst;

	rv = -1;

	mm = current->mm;

	address_src = src;
	address_dst = dst;

	ptl_src = NULL;
	ptl_dst = NULL;

	if (mm == NULL)
	{
			printk("mm NULL\n");
			return rv;
	}

	pgd_src = pgd_offset(mm,address_src);
	if (pgd_none(*pgd_src))
			return rv;
	if (pgd_bad(*pgd_src))
			return rv;
//printk("p1\n");
	p4d_src = p4d_offset(pgd_src,address_src);
	if (p4d_none(*p4d_src))
			return rv;
	if (p4d_bad(*p4d_src))
			return rv;
//printk("p2\n");

	pud_src = pud_offset(p4d_src,address_src);
	if (pud_none(*pud_src))
			return rv;
	if (pud_bad(*pud_src))
			return rv;
//printk("p3\n");

	pmd_src = pmd_offset(pud_src,address_src);
	if (pmd_none(*pmd_src))
			return rv;
	if (pmd_bad(*pmd_src))
			return rv;
//printk("p4\n");

	pgd_dst = pgd_offset(mm,address_dst);
	if (pgd_none(*pgd_dst))
			return rv;
	if (pgd_bad(*pgd_dst))
			return rv;
//printk("p5\n");

	p4d_dst = p4d_offset(pgd_dst,address_dst);
	if (p4d_none(*p4d_dst))
			return rv;
	if (p4d_bad(*p4d_dst))
			return rv;
//printk("p6\n");

	pud_dst = pud_offset(p4d_dst,address_dst);
	if (pud_none(*pud_dst))
			return rv;
	if (pud_bad(*pud_dst))
			return rv;
//printk("p7\n");

	pmd_dst = pmd_offset(pud_dst,address_dst);
	if (pmd_none(*pmd_dst))
			return rv;
	if (pmd_bad(*pmd_dst))
			return rv;
//printk("p8\n");

	//do we need lock?
//	pte_src = pte_offset_map(pmd_src,address_src);
	pte_src = pte_offset_map_lock(mm,pmd_src,address_src,&ptl_src);	
	if (pte_none(*pte_src) || !pte_present(*pte_src))
	{
			spin_unlock(ptl_src);
			__put_user(0,(char __user *)address_src);
			spin_lock(ptl_src);
			/*
			if (ptl_src != NULL)
				spin_unlock(ptl_src);
			return rv;
			*/
	}
	/*
	if (!pte_present(*pte_src))
	{
			if (ptl_src != NULL)
				spin_unlock(ptl_src);
			return rv;
	}
	*/
//	printk("p9\n");
//	pte_dst = pte_offset_map(pmd_dst,address_dst);
	if (pmd_dst->pmd == pmd_src->pmd)
		pte_dst = pte_offset_map(pmd_dst,address_dst);
	else
	{
		pte_dst = pte_offset_map_lock(mm,pmd_dst,address_dst,&ptl_dst);

	if (pte_none(*pte_dst) || !pte_present(*pte_dst))
	{
			spin_unlock(ptl_dst);
			__put_user(0,(char __user *)address_dst);
			spin_lock(ptl_dst);
			/*
			if (ptl_src != NULL)
				spin_unlock(ptl_src);
			if (ptl_dst != NULL)
				spin_unlock(ptl_dst);
			return rv;
			*/
	}
	/*
	if (!pte_present(*pte_dst))
	{
			if (ptl_src != NULL)
				spin_unlock(ptl_src);
			if (ptl_dst != NULL)
				spin_unlock(ptl_dst);
			return rv;
	}
	*/
	}
//	printk("ready\n");
//	printk("%lx",address_src);
//	printk("%lx %lx %lx %lx %lx",pgd_page_vaddr(pgd_src),p4d_page_vaddr(p4d_src),pud_page_vaddr(pud_src),pmd_page_vaddr(pmd),pte_page_vaddr(pte));

	pte_dst_end = (address_dst+PMD_SIZE) & PMD_MASK;
	pmd_dst_end = (address_dst+PUD_SIZE) & PUD_MASK;
	pud_dst_end = (address_dst+P4D_SIZE) & P4D_MASK;
	p4d_dst_end = (address_dst+PGDIR_SIZE) & PGDIR_MASK;

	pte_src_end = (address_src+PMD_SIZE) & PMD_MASK;
	pmd_src_end = (address_src+PUD_SIZE) & PUD_MASK;
	pud_src_end = (address_src+P4D_SIZE) & P4D_MASK;
	p4d_src_end = (address_src+PGDIR_SIZE) & PGDIR_MASK;

	if (size <= 0)
	{
			if (ptl_src != NULL)
				spin_unlock(ptl_src);
			if (ptl_dst != NULL)
				spin_unlock(ptl_dst);
			return 0;
	}

	pte_temp = *pte_dst;
	*pte_dst = *pte_src;
	*pte_src = pte_temp;

	barrier();
//on_each_cpu(__flush_tlb_one_user_pmc,(void*)&address_src,1);
//on_each_cpu(__flush_tlb_one_user_pmc,(void*)&address_dst,1);
printk("src %lu\n",address_src);
data.addr = address_src;
on_each_cpu(__flush_tlb_one_user_pmc,(void*) &data,1);
barrier();
printk("dst %lu\n",address_dst);
data.addr = address_dst;
on_each_cpu(__flush_tlb_one_user_pmc,(void*) &data,1);
barrier();


//	__flush_tlb_one_user(address_src);
//	__flush_tlb_one_user(address_dst);

	cnt = 1;

	while(cnt < size)
	{
			address_dst+=PAGE_SIZE;

			if (address_dst >= pte_dst_end)
			{
					pte_dst_end = (address_dst+PMD_SIZE) & PMD_MASK;
					if (address_dst >= pmd_dst_end)
					{
							pmd_dst_end = (address_dst+PUD_SIZE) & PUD_MASK;
							if (address_dst >= pud_dst_end)
							{
									pud_dst_end = (address_dst+P4D_SIZE) & P4D_MASK;
									if (address_dst >= p4d_dst_end)
									{
											p4d_dst_end = (address_dst+PGDIR_SIZE) & PGDIR_SIZE;
											//??? what if.. it will never happen
											p4d_dst = p4d_offset(pgd_dst,address_dst);
									}
									else
											++p4d_dst;
									pud_dst = pud_offset(p4d_dst,address_dst);
							}
							else
									++pud_dst;
							pmd_dst = pmd_offset(pud_dst,address_dst);
					}
					else
							++pmd_dst;
					if (ptl_src == NULL)
							ptl_src = ptl_dst;
					else if (ptl_dst != NULL)
						spin_unlock(ptl_dst);
//					pte_dst = pte_offset_map(pmd_dst,address_dst);
				if (pmd_dst->pmd != pmd_src->pmd)					
					pte_dst = pte_offset_map_lock(mm,pmd_dst,address_dst,&ptl_dst);				
				else
				{
					pte_dst = pte_offset_map(pmd_dst,address_dst);
					ptl_dst = NULL;	
				}
			}
			else
				++pte_dst;

			address_src+=PAGE_SIZE;

			if (address_src >= pte_src_end)
			{
					pte_src_end = (address_src+PMD_SIZE) & PMD_MASK;
					if (address_src >= pmd_src_end)
					{
							pmd_src_end = (address_src+PUD_SIZE) & PUD_MASK;
							if (address_src >= pud_src_end)
							{
									pud_src_end = (address_src+P4D_SIZE) & P4D_MASK;
									if (address_src >= p4d_src_end)
									{
											p4d_src_end = (address_src+PGDIR_SIZE) & PGDIR_SIZE;
											//??? what if.. it will never happen
											p4d_src = p4d_offset(pgd_src,address_src);
									}
									else
											++p4d_src;
									pud_src = pud_offset(p4d_src,address_src);
							}
							else
									++pud_src;
							pmd_src = pmd_offset(pud_src,address_src);
					}
					else
							++pmd_src;
					if (ptl_dst == NULL)
							ptl_dst = ptl_src;
					else if (ptl_src != NULL)
						spin_unlock(ptl_src);
//					pte_src = pte_offset_map(pmd_src,address_src);
				if (pmd_src->pmd != pmd_dst->pmd)					
					pte_src = pte_offset_map_lock(mm,pmd_src,address_src,&ptl_src);
				else
				{
					pte_src = pte_offset_map(pmd_src,address_src);
					ptl_src = NULL;
				}

			}
			else
				++pte_src;

			//really?
			pte_temp = *pte_dst;
			*pte_dst = *pte_src;
			*pte_src = pte_temp;

//			__flush_tlb_one_user(address_src);
//			__flush_tlb_one_user(address_dst);
/*
flush_tlb_mm_range(current->mm,address_src,address_src,PAGE_SHIFT,true);
flush_tlb_mm_range(current->mm,address_dst,address_dst,PAGE_SHIFT,true);
struct flush_tlb_info *info;
info = get_flush_tlb_info(current->mm,address_src,address_src,PAGE_SHIFT,true,inc_mm_tlb_get(current->mm));
on_each_cpu(flush_tlb_func_remote,(void *)info,1);
put_flush_tlb_info();
info = get_flush_tlb_info(current->mm,address_src,address_src,PAGE_SHIFT,true,inc_mm_tlb_get(current->mm));
on_each_cpu(flush_tlb_func_remote,(void *)info,1);
put_flush_tlb_info();
*/
//on_each_cpu(__native_flush_tlb_one_user,address_src,1);
//on_each_cpu(__native_flush_tlb_one_user,address_dst,1);

barrier();
//on_each_cpu(__flush_tlb_one_user_pmc,(void*) &address_src,1);
//on_each_cpu(__flush_tlb_one_user_pmc,(void*) &address_dst,1);
data.addr = address_src;
on_each_cpu(__flush_tlb_one_user_pmc,(void*) &data,1);
barrier();
data.addr = address_dst;
on_each_cpu(__flush_tlb_one_user_pmc,(void*) &data,1);

barrier();


			++cnt;
	}

	if (ptl_src != NULL)
		spin_unlock(ptl_src);
	if (ptl_dst != NULL)
		spin_unlock(ptl_dst);
//barrier();
printk("done\n");
	return 0;
}

//asmlinkage long sys_pmc(void *src, void *dst, unsigned long size) // no overlap
SYSCALL_DEFINE3(pmc, void*, src, void*, dst, unsigned long, size)
{

		unsigned long /*cnt,size2,*/src_addr,dst_addr;
		long rv;

//		printk("progamer mincheolgi choigo\n");

	src_addr = (unsigned long)src;
	dst_addr = (unsigned long)dst;

	if (src_addr % PAGE_SIZE != 0 || dst_addr % PAGE_SIZE != 0 || size % PAGE_SIZE != 0)
	{
//			memmove(dst_addr,src_addr,size);
			printk("align error %lx %lx %lx\n",src_addr,dst_addr,size);
			rv = -1;
//			return rv;
	}
	else
//		cnt = size / PAGE_SIZE;
		rv = dwc(src_addr,dst_addr,size / PAGE_SIZE);
//		rv = dwc(src,dst,cnt);

//		size2 = cnt*PAGE_SIZE;
//		if (size != size2)
//		memmove((void*)(dst_addr+size2),(void*)(src_addr+size2),size-size2);
		/*
		if (rv == -1)
				printk("dwc error\n");
				*/

		return rv;
}

SYSCALL_DEFINE4(pmc2, void*, src, void*, dst, void*, size, unsigned long, cnt)
{
		unsigned long i,_size,size2;
		void *_src,*_dst;
		long rv = 0;
		for (i=0;i<cnt;++i)
		{
//				printk("i %lu\n",i);
				copy_from_user(&_size,size+i*8,8); // every time?
				copy_from_user(&_src,src+i*8,8);
				copy_from_user(&_dst,dst+i*8,8);
//				printk("%lu\n",_size);
//				printk("%lx %lx %lu\n",(unsigned long)_src,(unsigned long)_dst,_size);
				
			if (((unsigned long)_src) % PAGE_SIZE == 0 && ((unsigned long)_dst) % PAGE_SIZE == 0)
			{
					size2 = _size%PAGE_SIZE;
//					printk("dwc %lx %lx %lu\n",src[i],dst[i],size[i]-size2);
					dwc((unsigned long)_src,(unsigned long)_dst,(_size-size2)/PAGE_SIZE);
					if (size2 > 0)
					{
							printk("memcpy %lu\n",size2);
//						memcpy((unsigned long)dst[i]+(size[i]-size2),(unsigned long)src[i]+(size[i]-size2),size2);
					}
			}
			else
			{
					printk("align error\n");
//					memcpy(dst[i],src[i],size[i]);
			}
			
		}
		return rv;
}

SYSCALL_DEFINE0(pmcf)
{
//		preempt_disable();
//		struct mm_struct *mm = current->mm;
//		flush_tlb_mm(current->mm);
//	flush_tlb_others(mm_cpumask(mm),mm,0UL,TLB_FLUSH_ALL);
//printk("p0\n");
printk("asid %lu\n",this_cpu_read(cpu_tlbstate.loaded_mm_asid));
	/*
	barrier();
//		flush_tlb_all();
	flush_tlb_all_pmc();		
	barrier();
	*/
//	printk("p1\n");
//	preempt_enable();
}

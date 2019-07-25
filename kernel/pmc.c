#include <linux/linkage.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>

#include <asm/tlbflush.h>

#include <asm/pgtable.h>
//#include <linux/mm.h>

long dwc(unsigned long src, unsigned long dst, unsigned long size) // double walk and change
{

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
	if (pte_none(*pte_src))
	{
			if (ptl_src != NULL)
				spin_unlock(ptl_src);
			return rv;
	}
	if (!pte_present(*pte_src))
	{
			if (ptl_src != NULL)
				spin_unlock(ptl_src);
			return rv;
	}
//	pte_dst = pte_offset_map(pmd_dst,address_dst);
	if (pmd_dst->pmd != pmd_src->pmd)
		pte_dst = pte_offset_map_lock(mm,pmd_dst,address_dst,&ptl_dst);
	else
		pte_dst = pte_offset_map(pmd_dst,address_dst);

	if (pte_none(*pte_dst))
	{
			if (ptl_src != NULL)
				spin_unlock(ptl_src);
			if (ptl_dst != NULL)
				spin_unlock(ptl_dst);
			return rv;
	}
	if (!pte_present(*pte_dst))
	{
			if (ptl_src != NULL)
				spin_unlock(ptl_src);
			if (ptl_dst != NULL)
				spin_unlock(ptl_dst);
			return rv;
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

	__flush_tlb_one_user(address_src);
	__flush_tlb_one_user(address_dst);

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

			__flush_tlb_one_user(address_src);
			__flush_tlb_one_user(address_dst);

			++cnt;
	}

	if (ptl_src != NULL)
		spin_unlock(ptl_src);
	if (ptl_dst != NULL)
		spin_unlock(ptl_dst);

	return 0;
}

//asmlinkage long sys_pmc(void *src, void *dst, unsigned long size) // no overlap
SYSCALL_DEFINE3(pmc, void*, src, void*, dst, unsigned long, size)
{

		unsigned long /*cnt,*/src_addr,dst_addr;
		long rv;

//		printk("progamer mincheolgi choigo\n");

	src_addr = (unsigned long)src;
	dst_addr = (unsigned long)dst;

	if ((src_addr % PAGE_SIZE != 0) || (dst_addr % PAGE_SIZE != 0) || (size % PAGE_SIZE != 0))
	{
			printk("align error %lx %lx %lx\n",src_addr,dst_addr,size);
			rv = -1;
	}
	else
		rv = dwc(src_addr,dst_addr,size/PAGE_SIZE);
//		cnt = size / PAGE_SIZE;
//		rv = dwc(src,dst,cnt);

//		memmove(dst+cnt*PAGE_SIZE,src+cnt*PAGE_SIZE,size-cnt*PAGE_SIZE);
		if (rv == -1)
				printk("dwc error\n");

		return 0;
}

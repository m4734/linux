#include <linux/linkage.h>
#include <linux/kernel.h>

#include <asm/pgtable.h>
//#include <linux/mm.h>

long dwc(void *src, void *dst, unsigned long size) // double walk and change
{

	struct	mm_struct *mm=NULL;//fix it

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

	rv = -1;

	address_src = (unsigned long)src;
	address_dst = (unsigned long)dst;

	pgd_src = pgd_offset(mm,address_src);
	if (pgd_none(*pgd_src))
			return rv;
	if (pgd_bad(*pgd_src))
			return rv;

	p4d_src = p4d_offset(pgd_src,address_src);
	if (p4d_none(*p4d_src))
			return rv;
	if (p4d_bad(*p4d_src))
			return rv;

	pud_src = pud_offset(p4d_src,address_src);
	if (pud_none(*pud_src))
			return rv;
	if (pud_bad(*pud_src))
			return rv;

	pmd_src = pmd_offset(pud_src,address_src);
	if (pmd_none(*pmd_src))
			return rv;
	if (pmd_bad(*pmd_src))
			return rv;

	pgd_dst = pgd_offset(mm,address_dst);
	if (pgd_none(*pgd_dst))
			return rv;
	if (pgd_bad(*pgd_dst))
			return rv;

	p4d_dst = p4d_offset(pgd_dst,address_dst);
	if (p4d_none(*p4d_dst))
			return rv;
	if (p4d_bad(*p4d_dst))
			return rv;

	pud_dst = pud_offset(p4d_dst,address_dst);
	if (pud_none(*pud_dst))
			return rv;
	if (pud_bad(*pud_dst))
			return rv;

	pmd_dst = pmd_offset(pud_dst,address_dst);
	if (pmd_none(*pmd_dst))
			return rv;
	if (pmd_bad(*pmd_dst))
			return rv;

	//do we need lock?
	pte_src = pte_offset_map(pmd_src,address_src);
	if (pte_none(*pte_src))
	{
			return rv;
	}
	if (!pte_present(*pte_src))
	{
			return rv;
	}
	pte_dst = pte_offset_map(pmd_dst,address_dst);
	if (pte_none(*pte_dst))
	{
			return rv;
	}
	if (!pte_present(*pte_dst))
	{
			return rv;
	}

	pte_dst_end = (address_dst+PMD_SIZE) & PMD_MASK;
	pmd_dst_end = (address_dst+PUD_SIZE) & PUD_MASK;
	pud_dst_end = (address_dst+P4D_SIZE) & P4D_MASK;
	p4d_dst_end = (address_dst+PGDIR_SIZE) & PGDIR_MASK;

	pte_src_end = (address_src+PMD_SIZE) & PMD_MASK;
	pmd_src_end = (address_src+PUD_SIZE) & PUD_MASK;
	pud_src_end = (address_src+P4D_SIZE) & P4D_MASK;
	p4d_src_end = (address_src+PGDIR_SIZE) & PGDIR_MASK;

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
					pte_dst = pte_offset_map(pmd_dst,address_dst);
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
					pte_src = pte_offset_map(pmd_src,address_src);
			}
			else
				++pte_src;

			//really?
			pte_temp = *pte_dst;
			*pte_dst = *pte_src;
			*pte_src = pte_temp;

			++cnt;
	}

	return 0;
}

asmlinkage long sys_pmc(void *src, void *dst, unsigned long size)
{

		unsigned long cnt;
		long rv;

		printk("progamer mincheolgi choigo\n");

		cnt = size / PAGE_SIZE;

		rv = dwc(src,dst,cnt);

		memmove(dst+cnt*PAGE_SIZE,src+cnt*PAGE_SIZE,size-cnt*PAGE_SIZE);
		if (rv == -1)
				printk("dwc error\n");

		return 0;
}

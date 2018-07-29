

# 设置属性

## 函数
通过ext4_set_acl设置或者删除文件的扩展属性，如果设置的值为空则删除该扩展属性。
```
ext4_set_acl
  ->ext4_journal_start
  ->__ext4_set_acl
    ->ext4_xattr_set_handle
      -> ext4_write_lock_xattr
      ->ext4_xattr_get_block
        ->
      ->ext4_xattr_ibody_find
        -> 
  ->ext4_journal_stop
```


```
ext4_journal_start
__ext4_journal_start
   ->__ext4_journal_start_sb
      ->jbd2__journal_start
        1. ->journal_current_handle
              ->current->journal_info
        ---
        2. ->new_handle
           ->start_this_handle
             ->current->journal_info = handle;
   
    
```

__ext4_journal_start_sb
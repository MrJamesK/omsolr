omsolr
==============

Output plugin for Rsyslog to communicate with a Solr instance. The plugin is written using the rsyslog 
"transactional" method, so that it never loses messages if the Solr instance is unavailable.  The plugin
has been thoroughly tested with rsyslog-5.8.4.


Installing omsolr
-------------------------
1. Apply the patch 'omsolr.patch' to the source of rsyslog:

>        cd path/to/rsyslog-5.8.4
>        patch -p1 -i omsolr.patch

2. Create the plugin directory structure for omsolr and copy over the contents.
        
>        mkdir patch/to/rsyslog-5.8.4/plugins/omsolr/
>        cp Makefile.am patch/to/rsyslog-5.8.4/plugins/omsolr/
>        cp omsolr.c patch/to/rsyslog-5.8.4/plugins/omsolr/

3. Regenerate autotools related files:

>        cd path/to/rsyslog-5.8.4
>        autoreconf

4. Add '--enable-omsolr' to ./configure switches.

>        ./configure \
>        --enable-omsolr \
>        <your-other-flags-here>

5. Build and install.

>        cd path/to/rsyslog-5.8.4
>        make
>        make install


Configuring omsolr
------------------
Add the following lines to rsyslog.conf

>    $ModLoad omsolr

>    $actionsolrurl <yoursolrhost:port>/solr/<corename>/update/
    
Define a valid Solr template, using XML, for communicating the data to your Solr schema.  Here is an example:

    $template SolrLog, "<add><doc><field name='hostname'>%FROMHOST%</field>
    <field name='facility'>%syslogfacility-text%</field><field name='msg'><![CDATA[%msg%]]></field>
    <field name='hostname'>%FROMHOST%</field><field name='priority'>%syslogpriority%</field>
    <field name='tag'>%syslogtag%</field><field name='program'>%programname%</field>
    <field name='severity'>%syslogseverity-text%</field>
    <field name='generated'>%timegenerated:::date-rfc3339%</field></doc></add>"

Please note that the template must include the <add> and <doc> fields.  These are NOT automatically 
added.  This provides additional flexibility so you can set arguments to the "add" specification.

Add in the action to talk to Sol.

>    :omsolr:;SolrLog

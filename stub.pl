#!/usr/bin/perl

use strict;
use warnings;
use diagnostics;
use sort 'stable';

use Getopt::Std;
use Data::Dumper;
use Term::ANSIColor;

my %options = ();
getopts("d:c:n:f:s:", \%options);

my $optionSaiDir        = $options{d} if defined $options{d};
my $optionClass         = $options{c} if defined $options{c};
my $optionNamespace     = $options{n} if defined $options{n};
my $optionFileName      = $options{f} if defined $options{f};
my $optionStub          = $options{s} if defined $options{s};

my $STUB = $optionStub;

my $DATA = "";
my @OBJECT_TYPES = ();
my %objectTypes = ();
my @APIS = ();
my @FUNCTIONS = ();
my %FUNCTIONS = ();
my @globalApis = ();
my @apiStructs = ();
my %apiStructs = ();
my %ENTRIES = ();

my $SOURCE_CONTENT = "";

my $warnings = 0;
my $errors = 0;

sub LogInfo
{
    print color('bright_green') . "@_" . color('reset') . "\n";
}

sub LogWarning
{
    $warnings++;
    print color('bright_yellow') . "WARNING: @_" . color('reset') . "\n";
}

sub LogError
{
    $errors++;
    print color('bright_red') . "ERROR: @_" . color('reset') . "\n";
    exit 1;
}

sub WriteFile
{
    my ($file, $content) = @_;

    open (F, ">", $file) or die "$0: open $file $!";

    print F $content;

    close F;
}

sub Write
{
    my $content = shift;

    my $ident = ""; #GetIdent($content);

    my $line = $ident . $content . "\n";

    $line = "\n" if $content eq "";

    $SOURCE_CONTENT .= $line;
}

sub GetFunctionCamelCaseName
{
    my $fun = shift;

    $fun =~ s/^sai_//;

    my @tokens = split/_/,$fun;

    @tokens = map{ucfirst}@tokens;

    shift @tokens if $tokens[0] eq "api";

    $tokens[0] = lcfirst $tokens[0];

    $fun = join("",@tokens);

    return $fun;
}

sub GetData
{
    $DATA = `cat $optionSaiDir/inc/sai*.h $optionSaiDir/experimental/sai*.h`;
}

sub SanitizeData
{
    $DATA =~ s/SAI_OBJECT_TYPE_\w*(START|END|NULL|MAX)//gms;
    $DATA =~ s/SAI_API_\w*(START|END|UNSPECIFIED|MAX|EXTENSIONS_RANGE_BASE)//gms;
}

sub ExtractData
{
    @OBJECT_TYPES = $DATA =~ /^\s+SAI_OBJECT_TYPE\_(\w+)/gms;
    @APIS = $DATA =~ /^\s+SAI_API\_(\w+)/gms;

    for my $ot (@OBJECT_TYPES)
    {
        $objectTypes{lc$ot} = $ot;
    }

    @FUNCTIONS = $DATA =~ /(typedef sai_status_t \(\*(?:sai_\w+_fn)\).*?\))/gms;

    @globalApis = $DATA =~ /^((?:sai_\w+)\s+(?:sai_\w+).+?\))/gms;

    for my $fun (@FUNCTIONS)
    {
        next if not $fun =~ /sai_\w+\s+.*?(sai_\w+)/gms;

        $FUNCTIONS{$1} = $fun;
    }

    @apiStructs = $DATA =~ /(_sai_\w+_api_t.+?sai_\w+_api_t)/gms;

    for my $struct (@apiStructs)
    {
        next if not $struct =~ /sai_(\w+)_api_t/;

        $apiStructs{$1} = $struct;
    }

    my @entries = $DATA =~ /\bsai_(\w+_entry)_t\b/gms;

    for my $e (@entries)
    {
        $ENTRIES{uc$e} = $e;
    }
}

sub Entry
{
    my $i = shift;
    $i =~ s/entries/entry/;
    return $i;
}

sub GetFunctionName
{
    my $fun = shift;

    my $OT = "";
    my $bulk = 0;
    my $entry = 0;

    if ($fun =~ /(get|clear)_(\w+)_(stats(_ext)?)/ and defined $objectTypes{$2})
    {
        $OT = $objectTypes{$2};
        $fun = "$1_$3";
    }
    elsif ($fun =~ /(create|remove|set|get)_(\w+_entries)/ and defined $objectTypes{Entry($2)})
    {
        $bulk = 1;
        $OT = $objectTypes{Entry($2)};
        $fun = "bulk_$1";
    }
    elsif ($fun =~ /(set|get)_(\w+)s_attribute/ and defined $objectTypes{$2})
    {
        $bulk = 1;
        $OT = $objectTypes{$2};
        $fun = "bulk_$1";
    }
    elsif ($fun =~ /(set|get)_(\w+)_attribute/ and defined $objectTypes{$2})
    {
        $OT = $objectTypes{$2};
        $fun = "$1";
    }
    elsif ($fun =~ /(create|remove|set|get)_(\w+)s(_attribute)?/ and defined $objectTypes{$2})
    {
        $bulk = 1;
        $OT = $objectTypes{$2};
        $fun = "bulk_$1";
    }
    elsif ($fun =~ /(create|remove|set|get)_(\w+)(_attribute)?/ and defined $objectTypes{$2})
    {
        $OT = $objectTypes{$2};
        $fun = "$1";
    }
    elsif ($fun =~ /(create|remove|set|get)_(\w+)(_attribute)?/ and defined $objectTypes{$2})
    {
        $OT = $objectTypes{$2};
        $fun = "$1";
    }
    else
    {
        LogInfo "Unique function $fun";
    }

    $entry = 1 if defined $ENTRIES{$OT};

    my $f = GetFunctionCamelCaseName($fun);

    return ($OT, $f, $entry, $bulk);
}

sub CreateApiStricts()
{
    Write "";
    Write "/* ==== API STRUCTS === */";
    Write "";

    for my $API (@APIS)
    {
        my $api = lc $API;

        Write "";
        Write "/* SAI APIS for $API */";
        Write "";

        my $struct = $apiStructs{$api};

        LogError "api $api not found" if not defined $struct;

        while ($struct =~ /(sai_\w+_fn)\s+(\w+)/gms)
        {
            my $type = $1;
            my $funname = $2;

            my $fun = $FUNCTIONS{$type};

            LogError "function $fun not found" if not defined $fun;

            next if not $fun =~ /(sai_\w+).+\((.+?)\)/gms;

            my $rettype = $1;
            my $params = $2;
            my @par = $params =~ /(\w+,|\w+$)/gms;

            my ($OT, $fname, $entry, $bulk) = GetFunctionName($funname);

            $par[0] = "switch_id,SAI_NULL_OBJECT_ID," if $OT eq "SWITCH" and $bulk == 0 and $fname eq "create";

            Write "static $rettype ${STUB}_$funname($params)";
            Write "{";
            Write "    SWSS_LOG_ENTER();";
            Write "";

            if ($fname =~ /(clearPortAllStats|removeAllNeighborEntries|recvHostifPacket|sendHostifPacket|allocateHostifPacket|freeHostifPacket)/)
            {
                Write "    SWSS_LOG_ERROR(\"FIXME, no implementation for $fname!\");";
                Write "    return SAI_STATUS_NOT_IMPLEMENTED;";
                Write "}";
                next;
            }

            Write "    return $STUB" . "->$fname(@par);" if $OT eq "";
            Write "    return $STUB" . "->$fname(@par);" if $OT ne "" and $bulk == 1 and $entry == 1;
            Write "    return $STUB" . "->$fname(@par);" if $OT ne "" and $bulk == 0 and $entry == 1;
            Write "    return $STUB" . "->$fname((sai_object_type_t)(SAI_OBJECT_TYPE_$OT),@par);" if $OT ne "" and $bulk == 0 and $entry == 0;
            Write "    return $STUB" . "->$fname((sai_object_type_t)(SAI_OBJECT_TYPE_$OT),@par);" if $OT ne "" and $bulk == 1 and $entry == 0;
            Write "}";
            Write "";
        }

        Write "static sai_${api}_api_t ${STUB}_${api} = {";

        while ($struct =~ /(sai_\w+_fn)\s+(\w+)/gms)
        {
            my $type = $1;
            my $funname = $2;

            Write "    .$funname = ${STUB}_$funname,";
        }

        Write "};";
        Write "";
    }
}

sub CreateHeader
{
    Write "";
    Write "/* DO NOT MODIFY, FILE AUTO GENERATED */";
    Write "";
    Write "extern \"C\" {";
    Write "#include \"sai.h\"";
    Write "#include \"saiextensions.h\"";
    Write "}";
    Write "#include \"meta/SaiInterface.h\"";
    Write "#include \"$optionClass.h\"";
    Write "#include \"swss/logger.h\"";
    Write "#include <memory>";
    Write "";
    Write "static std::shared_ptr<sairedis::SaiInterface> $STUB = std::make_shared<$optionNamespace" . "::$optionClass>();";
    Write ""
}

sub CreateApiQuery
{
    Write "";
    Write "/* ==== API QUERY === */";
    Write "";
    Write "sai_status_t sai_api_query(";
    Write "        _In_ sai_api_t api,";
    Write "        _Out_ void** api_method_table)";
    Write "{";
    Write "    SWSS_LOG_ENTER();";
    Write "";
    Write "    if (api_method_table == NULL)";
    Write "    {";
    Write "        SWSS_LOG_ERROR(\"NULL method table passed to SAI API initialize\");";
    Write "";
    Write "        return SAI_STATUS_INVALID_PARAMETER;";
    Write "    }";
    Write "";
    Write "    if (api == SAI_API_UNSPECIFIED)";
    Write "    {";
    Write "        SWSS_LOG_ERROR(\"api ID is unspecified api\");";
    Write "";
    Write "        return SAI_STATUS_INVALID_PARAMETER;";
    Write "    }";
    Write "";
    Write "    switch((int)api)";
    Write "    {";

    for my $API (@APIS)
    {
        my $api = lc $API;

        Write "        case SAI_API_$API:";
        Write "            *api_method_table = (void**)&${STUB}_${api};";
        Write "            return SAI_STATUS_SUCCESS;";
    }

    Write "        default:";
    Write "            break;";
    Write "    }";
    Write "";
    Write "    SWSS_LOG_ERROR(\"Invalid API type %d\", api);";
    Write "";
    Write "    return SAI_STATUS_INVALID_PARAMETER;";
    Write "}";
    Write "";
}

sub CreateGlobalApis
{
    Write "";
    Write "/* ==== GLOBAL APIS === */";
    Write "";

    for my $api (sort @globalApis)
    {
        next if not $api =~ /(sai_\w+)\s+(sai_\w+).*\((.+?)\)/gms;

        my $rettype = $1;
        my $funname = $2;
        my $params = $3;

        next if $funname eq "sai_api_query";

        my @par = $params =~ /(\w+,|\w+$)/gms;

        my $fun = GetFunctionCamelCaseName($funname);

        $par[0] = "" if $par[0] eq "void";

        Write "$rettype $funname($params)";
        Write "{";
        Write "    SWSS_LOG_ENTER();";
        Write "";

        if ($fun =~ /(bulkObjectClearStats|bulkObjectGetStats|dbgGenerateDump|getMaximumAttributeCount|getObjectKey|bulkGetAttribute|dbgGenerateDump|tamTelemetryGetData|getObjectCount|queryObjectStage)/)
        {
            Write "    SWSS_LOG_ERROR(\"FIXME, no implementation for $fun!\");";
            Write "    return SAI_STATUS_NOT_IMPLEMENTED;";
            Write "}";
            next;
        }

        Write "    return $STUB" . "->$fun(@par);";
        Write "}";
        Write "";
    }
}

#
# MAIN
#


LogError "Option SaiDir not specifird (-d)" if not defined $optionSaiDir;
LogError "Option Class not specifird (-c)" if not defined $optionClass;
LogError "Option Namespace not specifird (-n)" if not defined $optionNamespace;
LogError "Option FileName not specifird (-f)" if not defined $optionFileName;
LogError "Option Stub not specifird (-s)" if not defined $optionStub;

LogInfo "optionSaiDir     = $optionSaiDir   ";
LogInfo "optionClass      = $optionClass    ";
LogInfo "optionNamespace  = $optionNamespace";
LogInfo "optionFileName   = $optionFileName ";
LogInfo "optionStub       = $optionStub     ";

GetData();
SanitizeData();
ExtractData();
CreateHeader();
CreateApiStricts();
CreateApiQuery();
CreateGlobalApis();
WriteFile($optionFileName,$SOURCE_CONTENT);
